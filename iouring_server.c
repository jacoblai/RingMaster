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
#include <sys/resource.h>

#define QUEUE_DEPTH 32768
#define MAX_CONNECTIONS 1000000
#define MAX_MESSAGE_LEN 2048

int max_connections = MAX_CONNECTIONS;
volatile int keep_running = 1;
static struct io_uring ring;
static int server_socket;
static struct connection** connections = NULL;

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

static int write_to_file(const char *filename, const char *value) {
    FILE *file = fopen(filename, "w");
    if (file == NULL) {
        perror("Error opening file");
        return -1;
    }
    fprintf(file, "%s", value);
    fclose(file);
    return 0;
}

static int set_system_limits() {
    struct rlimit rl;
    int warnings = 0;

    // 尝试设置最大文件描述符数
    rl.rlim_cur = rl.rlim_max = MAX_CONNECTIONS;
    if (setrlimit(RLIMIT_NOFILE, &rl) == -1) {
        fprintf(stderr, "Warning: setrlimit(RLIMIT_NOFILE) failed: %s\n", strerror(errno));
        warnings++;
    }

    // 尝试设置最大进程数
    rl.rlim_cur = rl.rlim_max = MAX_CONNECTIONS;
    if (setrlimit(RLIMIT_NPROC, &rl) == -1) {
        fprintf(stderr, "Warning: setrlimit(RLIMIT_NPROC) failed: %s\n", strerror(errno));
        warnings++;
    }

    // 尝试设置 TCP 相关参数
    if (write_to_file("/proc/sys/net/ipv4/tcp_max_syn_backlog", "65536") == -1) {
        fprintf(stderr, "Warning: Failed to set tcp_max_syn_backlog\n");
        warnings++;
    }
    if (write_to_file("/proc/sys/net/ipv4/tcp_fin_timeout", "10") == -1) {
        fprintf(stderr, "Warning: Failed to set tcp_fin_timeout\n");
        warnings++;
    }
    if (write_to_file("/proc/sys/net/ipv4/tcp_tw_reuse", "1") == -1) {
        fprintf(stderr, "Warning: Failed to set tcp_tw_reuse\n");
        warnings++;
    }

    // 尝试增加系统范围的文件描述符限制
    if (write_to_file("/proc/sys/fs/file-max", "2097152") == -1) {
        fprintf(stderr, "Warning: Failed to set file-max\n");
        warnings++;
    }

    if (warnings > 0) {
        fprintf(stderr, "Some system limits could not be set. The server may not perform optimally.\n");
        fprintf(stderr, "To set these limits, try running the program with sudo or as root.\n");
    } else {
        printf("System limits set for high concurrency\n");
    }

    return 0;  // 即使有警告也继续运行
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

    char *buf = &conn->read_buffer.buffer[conn->read_buffer.write_index & RING_BUFFER_MASK];
    size_t buf_size = ring_buffer_free_space(&conn->read_buffer);

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

    char *buf = &conn->write_buffer.buffer[conn->write_buffer.read_index & RING_BUFFER_MASK];

    io_uring_prep_send(sqe, conn->fd, buf, data_size, 0);
    io_uring_sqe_set_data(sqe, conn);
    conn->state = CONN_STATE_WRITING;
    return 0;
}

static void close_and_free_connection(struct connection *conn) {
    if (!conn) return;

    int fd = conn->fd;
    if (fd >= 0 && fd < max_connections) {
        if (connections[fd] == conn) {
            connections[fd] = NULL;
            if (on_disconnect) {
                on_disconnect(&conn->addr);
            }
            close(fd);
            put_connection(conn);
        }
    }
}

static void handle_client_data(struct connection *conn, ssize_t bytes_read) {
    if (on_data) {
        char temp_buffer[RING_BUFFER_SIZE];
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
        atomic_fetch_add(&conn->read_buffer.write_index, cqe->res);
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

    struct connection *conn = get_connection();
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

    set_system_limits();  // 移除了错误检查，因为我们总是返回 0

    connections = calloc(max_connections, sizeof(struct connection*));
    if (!connections) {
        perror("Failed to allocate connections array");
        return 1;
    }

    init_pool();

    server_socket = setup_listening_socket(port);
    if (server_socket < 0) {
        fprintf(stderr, "Failed to set up listening socket\n");
        free(connections);
        return 1;
    }

    if (io_uring_queue_init(QUEUE_DEPTH, &ring, 0) < 0) {
        perror("io_uring_queue_init");
        close(server_socket);
        free(connections);
        return 1;
    }

    if (add_accept_request() < 0) {
        fprintf(stderr, "Failed to add initial accept request\n");
        io_uring_queue_exit(&ring);
        close(server_socket);
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

    free(connections);
    return 0;
}