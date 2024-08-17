#include "iouring_server.h"
#include "memory_pool.h"
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
#include <sys/resource.h>

#define QUEUE_DEPTH 32768
#define MAX_MESSAGE_LEN 2048
#define INITIAL_BUFFER_SIZE 1024

int max_connections = MAX_CONNECTIONS;
static struct io_uring ring;
static int server_socket;
static struct connection** connections = NULL;

static on_connect_cb on_connect = NULL;
static on_disconnect_cb on_disconnect = NULL;
static on_data_cb on_data = NULL;

static volatile int keep_running = 1;

void set_on_connect(on_connect_cb cb) { on_connect = cb; }
void set_on_disconnect(on_disconnect_cb cb) { on_disconnect = cb; }
void set_on_data(on_data_cb cb) { on_data = cb; }

static void sigint_handler(int sig) {
    (void)sig;
    keep_running = 0;
}

static int set_non_blocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int set_system_limits() {
    struct rlimit rl;

    rl.rlim_cur = rl.rlim_max = MAX_CONNECTIONS * 2;
    if (setrlimit(RLIMIT_NOFILE, &rl) == -1) {
        perror("setrlimit(RLIMIT_NOFILE)");
        fprintf(stderr, "Warning: Could not increase file descriptor limit. Continuing with system default.\n");
        if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
            printf("Current file descriptor limit: %llu\n", (unsigned long long)rl.rlim_cur);
        }
        return 0;
    }

    printf("File descriptor limit set to %llu\n", (unsigned long long)rl.rlim_cur);
    return 0;
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

    if (set_non_blocking(sock) < 0) {
        perror("set_non_blocking");
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
    io_uring_sqe_set_data(sqe, (void*)(intptr_t)-1);  // Use -1 to identify accept completions
    return 0;
}

static struct connection* create_connection(int fd) {
    struct connection* conn = get_connection();
    if (!conn) {
        fprintf(stderr, "Failed to allocate connection from pool\n");
        return NULL;
    }

    conn->fd = fd;
    conn->state = CONN_STATE_READING;

    return conn;
}

static void close_and_free_connection(struct connection *conn) {
    if (!conn) return;

    int fd = conn->fd;
    if (fd >= 0 && fd < max_connections) {
        if (connections[fd] == conn) {
            connections[fd] = NULL;
            close(fd);
            put_connection(conn);
            if (on_disconnect) {
                on_disconnect(&conn->addr);
            }
        }
    }
}

static int add_read_request(struct connection *conn) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
    if (!sqe) {
        fprintf(stderr, "Could not get SQE for read.\n");
        return -1;
    }

    size_t buf_size = ring_buffer_free_space(&conn->read_buffer);
    if (buf_size == 0) {
        if (ring_buffer_write(&conn->read_buffer, "", 0) != 0) {  // This will trigger a resize
            fprintf(stderr, "Failed to resize read buffer.\n");
            return -1;
        }
        buf_size = ring_buffer_free_space(&conn->read_buffer);
    }

    size_t write_index = atomic_load(&conn->read_buffer.write_index) % conn->read_buffer.capacity;
    char* buf = &conn->read_buffer.buffer[write_index];

    io_uring_prep_recv(sqe, conn->fd, buf, buf_size, 0);
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

    size_t data_size = ring_buffer_used_space(&conn->write_buffer);
    if (data_size == 0) {
        return add_read_request(conn);
    }

    size_t read_index = atomic_load(&conn->write_buffer.read_index) % conn->write_buffer.capacity;
    char* buf = &conn->write_buffer.buffer[read_index];

    io_uring_prep_send(sqe, conn->fd, buf, data_size, 0);
    io_uring_sqe_set_data(sqe, conn);
    conn->state = CONN_STATE_WRITING;
    return 0;
}

static void handle_client_data(struct connection *conn, ssize_t bytes_read) {
    atomic_fetch_add(&conn->read_buffer.write_index, bytes_read);

    if (on_data) {
        char temp_buffer[MAX_MESSAGE_LEN];
        size_t read_size = ring_buffer_read(&conn->read_buffer, temp_buffer, bytes_read);
        on_data(&conn->addr, temp_buffer, read_size);

        ring_buffer_write(&conn->write_buffer, temp_buffer, read_size);
    }

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
        handle_client_data(conn, cqe->res);
    } else if (conn->state == CONN_STATE_WRITING) {
        atomic_fetch_add(&conn->write_buffer.read_index, cqe->res);
        add_read_request(conn);
    }
}

static void handle_accept(struct io_uring_cqe *cqe) {
    int client_socket = cqe->res;
    if (client_socket < 0) {
        fprintf(stderr, "Accept failed: %s\n", strerror(-client_socket));
        return;
    }

    if (client_socket >= max_connections) {
        fprintf(stderr, "Client socket %d is out of range\n", client_socket);
        close(client_socket);
        return;
    }

    if (set_non_blocking(client_socket) < 0) {
        fprintf(stderr, "Failed to set client socket to non-blocking\n");
        close(client_socket);
        return;
    }

    struct connection *conn = create_connection(client_socket);
    if (!conn) {
        close(client_socket);
        return;
    }

    connections[client_socket] = conn;
    socklen_t addr_len = sizeof(conn->addr);
    getpeername(client_socket, (struct sockaddr*)&conn->addr, &addr_len);

    if (on_connect) {
        on_connect(&conn->addr);
    }

    add_read_request(conn);
    add_accept_request();
}

static void handle_completion_event(struct io_uring_cqe *cqe) {
    void *user_data = io_uring_cqe_get_data(cqe);
    if (user_data == (void*)(intptr_t)-1) {
        handle_accept(cqe);
    } else {
        handle_client_io(cqe);
    }
}

int start_server(int port) {
    printf("Starting server on port %d\n", port);

    signal(SIGINT, sigint_handler);

    if (set_system_limits() == -1) {
        fprintf(stderr, "Failed to set system limits\n");
        return 1;
    }

    connections = calloc(max_connections, sizeof(struct connection*));
    if (!connections) {
        perror("Failed to allocate connections array");
        return 1;
    }

    init_connection_pool(1000);  // Initialize connection pool with 1000 initial connections

    server_socket = setup_listening_socket(port);
    if (server_socket < 0) {
        fprintf(stderr, "Failed to set up listening socket\n");
        clean_connection_pool();
        free(connections);
        return 1;
    }

    if (io_uring_queue_init(QUEUE_DEPTH, &ring, 0) < 0) {
        perror("io_uring_queue_init");
        close(server_socket);
        clean_connection_pool();
        free(connections);
        return 1;
    }

    if (add_accept_request() < 0) {
        fprintf(stderr, "Failed to add initial accept request\n");
        io_uring_queue_exit(&ring);
        close(server_socket);
        clean_connection_pool();
        free(connections);
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

    for (int i = 0; i < max_connections; i++) {
        if (connections[i]) {
            close_and_free_connection(connections[i]);
        }
    }

    clean_connection_pool();
    free(connections);
    return 0;
}