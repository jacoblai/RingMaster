#include "iouring_server.h"
#include "connection_pool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <liburing.h>
#include <signal.h>

#define QUEUE_DEPTH 256

volatile int keep_running = 1;
static struct io_uring ring;
static int server_socket;
static struct connection* connections[MAX_CONNECTIONS];

static on_connect_cb on_connect = NULL;
static on_disconnect_cb on_disconnect = NULL;
static on_data_cb on_data = NULL;

void set_on_connect(on_connect_cb cb) { on_connect = cb; }
void set_on_disconnect(on_disconnect_cb cb) { on_disconnect = cb; }
void set_on_data(on_data_cb cb) { on_data = cb; }

static void sigint_handler(int sig) {
    (void)sig;
    printf("Received SIGINT, shutting down...\n");
    keep_running = 0;
}

static int setup_listening_socket(int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("socket");
        return -1;
    }

    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        perror("setsockopt(SO_REUSEADDR)");
        close(sock);
        return -1;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(sock);
        return -1;
    }

    if (listen(sock, SOMAXCONN) < 0) {
        perror("listen");
        close(sock);
        return -1;
    }

    return sock;
}

static int add_accept_request() {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    if (!sqe) {
        fprintf(stderr, "Could not get SQE for accept.\n");
        return -1;
    }

    io_uring_prep_accept(sqe, server_socket, NULL, NULL, 0);
    io_uring_sqe_set_data(sqe, (void*)-1);  // Use -1 to identify accept completions
    return 0;
}

static int add_read_request(struct connection *conn) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    if (!sqe) {
        fprintf(stderr, "Could not get SQE for read.\n");
        return -1;
    }

    io_uring_prep_recv(sqe, conn->fd, conn->read_buffer, conn->read_buffer_size, 0);
    io_uring_sqe_set_data(sqe, conn);
    conn->state = CONN_STATE_READING;
    return 0;
}

static int add_write_request(struct connection *conn) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    if (!sqe) {
        fprintf(stderr, "Could not get SQE for write.\n");
        return -1;
    }

    io_uring_prep_send(sqe, conn->fd, conn->write_buffer, conn->bytes_to_write, 0);
    io_uring_sqe_set_data(sqe, conn);
    conn->state = CONN_STATE_WRITING;
    return 0;
}

static void close_and_free_connection(struct connection *conn) {
    if (!conn) return;

    int fd = conn->fd;
    if (fd >= 0 && fd < MAX_CONNECTIONS) {
        if (connections[fd] == conn) {
            connections[fd] = NULL;
            if (on_disconnect) {
                on_disconnect(&conn->addr);
            }
            close(fd);
            put_connection(conn);  // Return to the pool
        }
    }
}

static void handle_client_data(struct connection *conn, ssize_t bytes_read) {
    if (on_data) {
        on_data(&conn->addr, conn->read_buffer, bytes_read);
    }

    // Copy data to write buffer
    memcpy(conn->write_buffer, conn->read_buffer, bytes_read);
    conn->bytes_to_write = bytes_read;

    // Clear read buffer
    memset(conn->read_buffer, 0, conn->read_buffer_size);
    conn->bytes_read = 0;

    // Prepare to write data back to client
    add_write_request(conn);
}

static void handle_client_io(struct io_uring_cqe *cqe) {
    struct connection *conn = (struct connection *)io_uring_cqe_get_data(cqe);
    if (!conn) {
        fprintf(stderr, "Invalid connection data in completion event.\n");
        return;
    }

    if (cqe->res <= 0) {
        if (cqe->res < 0) {
            fprintf(stderr, "Client IO error: %s\n", strerror(-cqe->res));
        }
        close_and_free_connection(conn);
        return;
    }

    if (conn->state == CONN_STATE_READING) {
        conn->bytes_read += cqe->res;
        handle_client_data(conn, conn->bytes_read);
    } else if (conn->state == CONN_STATE_WRITING) {
        // Clear write buffer
        memset(conn->write_buffer, 0, conn->write_buffer_size);
        conn->bytes_to_write = 0;

        // Prepare to read more data
        add_read_request(conn);
    }
}

static void handle_accept(struct io_uring_cqe *cqe) {
    int client_socket = cqe->res;
    if (client_socket < 0) {
        fprintf(stderr, "Accept failed: %s\n", strerror(-client_socket));
        return;
    }

    if (client_socket >= MAX_CONNECTIONS) {
        fprintf(stderr, "Client socket %d is out of range\n", client_socket);
        close(client_socket);
        return;
    }

    struct connection *conn = get_connection();  // Get from the pool
    if (!conn) {
        fprintf(stderr, "Failed to get connection from pool\n");
        close(client_socket);
        return;
    }

    conn->fd = client_socket;
    socklen_t addr_len = sizeof(conn->addr);
    getpeername(client_socket, (struct sockaddr*)&conn->addr, &addr_len);

    connections[client_socket] = conn;

    if (on_connect) {
        on_connect(&conn->addr);
    }

    add_read_request(conn);
    add_accept_request();
}

static void handle_completion_event(struct io_uring_cqe *cqe) {
    void *user_data = io_uring_cqe_get_data(cqe);
    if (user_data == (void*)-1) {
        handle_accept(cqe);
    } else {
        handle_client_io(cqe);
    }
}

int start_server(int port) {
    printf("Starting server on port %d\n", port);

    signal(SIGINT, sigint_handler);

    init_pool();  // Initialize the connection pool

    server_socket = setup_listening_socket(port);
    if (server_socket < 0) {
        fprintf(stderr, "Failed to set up listening socket\n");
        return 1;
    }

    if (io_uring_queue_init(QUEUE_DEPTH, &ring, 0) < 0) {
        perror("io_uring_queue_init");
        close(server_socket);
        return 1;
    }

    if (add_accept_request() < 0) {
        fprintf(stderr, "Failed to add initial accept request\n");
        io_uring_queue_exit(&ring);
        close(server_socket);
        return 1;
    }

    printf("Server started. Press Ctrl+C to stop.\n");

    while (keep_running) {
        io_uring_submit(&ring);

        struct io_uring_cqe *cqe;
        int ret = io_uring_wait_cqe(&ring, &cqe);
        if (ret < 0) {
            perror("io_uring_wait_cqe");
            if (ret == -EINTR) {
                continue;
            }
            break;
        }

        handle_completion_event(cqe);
        io_uring_cqe_seen(&ring, cqe);
    }

    printf("Shutting down server...\n");
    io_uring_queue_exit(&ring);
    close(server_socket);

    for (int i = 0; i < MAX_CONNECTIONS; i++) {
        if (connections[i]) {
            close_and_free_connection(connections[i]);
        }
    }

    return 0;
}