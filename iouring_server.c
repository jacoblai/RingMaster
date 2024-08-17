#include "iouring_server.h"
#include "error.h"
#include "resource_manager.h"
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

static volatile sig_atomic_t keep_running = 1;

static on_connect_cb on_connect = NULL;
static on_disconnect_cb on_disconnect = NULL;
static on_data_cb on_data = NULL;

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

static struct iovec *bufs;
static char *buf_base;

static int setup_buffers(struct io_uring *ring) {
    bufs = calloc(BUFFER_COUNT, sizeof(struct iovec));
    if (!bufs) {
        handle_error(ERR_MEMORY_ALLOC_FAILED, "Failed to allocate buffer array");
        return -1;
    }

    buf_base = malloc(BUFFER_SIZE * BUFFER_COUNT);
    if (!buf_base) {
        handle_error(ERR_MEMORY_ALLOC_FAILED, "Failed to allocate buffer base");
        free(bufs);  // Free the previously allocated buffer array
        bufs = NULL;
        return -1;
    }

    for (int i = 0; i < BUFFER_COUNT; i++) {
        bufs[i].iov_base = buf_base + (i * BUFFER_SIZE);
        bufs[i].iov_len = BUFFER_SIZE;
    }

    int ret = io_uring_register_buffers(ring, bufs, BUFFER_COUNT);
    if (ret) {
        handle_error(ERR_URING_INIT_FAILED, "Failed to register buffers");
        free(bufs);
        free(buf_base);
        bufs = NULL;
        buf_base = NULL;
        return -1;
    }

    return 0;
}

static int add_accept_request(struct io_uring *ring, int server_socket) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        handle_error(ERR_URING_QUEUE_FULL, "Could not get SQE for accept");
        return -1;
    }

    io_uring_prep_accept(sqe, server_socket, NULL, NULL, 0);
    io_uring_sqe_set_data(sqe, (void*)(intptr_t)-1);  // Use -1 to identify accept completions
    return 0;
}

static struct connection* create_connection(ResourceManager *rm, int fd) {
    struct connection* conn = memory_pool_alloc(rm->connection_pool);
    if (!conn) {
        handle_error(ERR_MEMORY_ALLOC_FAILED, "Failed to allocate connection from pool");
        return NULL;
    }

    memset(conn, 0, sizeof(struct connection));
    conn->fd = fd;
    conn->state = CONN_STATE_READING;
    conn->buffer_id = -1;  // 初始化为-1，表示未分配缓冲区

    ring_buffer_init(&conn->read_buffer, BUFFER_SIZE);
    ring_buffer_init(&conn->write_buffer, BUFFER_SIZE);

    if (conn->read_buffer.buffer == NULL || conn->write_buffer.buffer == NULL) {
        handle_error(ERR_MEMORY_ALLOC_FAILED, "Failed to initialize buffers");
        memory_pool_free(rm->connection_pool, conn);
        return NULL;
    }

    return conn;
}

static void close_and_free_connection(ResourceManager *rm, struct connection *conn) {
    if (!conn) return;

    int fd = conn->fd;
    if (fd >= 0 && fd < rm->max_connections) {
        if (rm->connections[fd] == conn) {
            rm->connections[fd] = NULL;
            close(fd);

            struct sockaddr_in client_addr = conn->addr;

            int saved_errno = errno;

            if (on_disconnect) {
                on_disconnect(&client_addr);
            }

            errno = saved_errno;

            ring_buffer_destroy(&conn->read_buffer);
            ring_buffer_destroy(&conn->write_buffer);
            memory_pool_free(rm->connection_pool, conn);
        }
    }
}

static int add_read_request(struct io_uring *ring, struct connection *conn) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        handle_error(ERR_URING_QUEUE_FULL, "Could not get SQE for read");
        return -1;
    }

    // 使用固定缓冲区进行读取
    int buf_index = conn->buffer_id;
    if (buf_index == -1) {
        // 如果连接还没有分配缓冲区，我们需要分配一个
        for (int i = 0; i < BUFFER_COUNT; i++) {
            if (bufs[i].iov_base != NULL) {
                buf_index = i;
                conn->buffer_id = i;
                break;
            }
        }
        if (buf_index == -1) {
            handle_error(ERR_RESOURCE_INIT_FAILED, "No available buffer");
            return -1;
        }
    }

    io_uring_prep_read_fixed(sqe, conn->fd, bufs[buf_index].iov_base, BUFFER_SIZE, 0, buf_index);
    io_uring_sqe_set_data(sqe, conn);
    conn->state = CONN_STATE_READING;
    return 0;
}

static int add_write_request(struct io_uring *ring, struct connection *conn) {
    struct io_uring_sqe *sqe = io_uring_get_sqe(ring);
    if (!sqe) {
        handle_error(ERR_URING_QUEUE_FULL, "Could not get SQE for write");
        return -1;
    }

    size_t data_size = ring_buffer_used_space(&conn->write_buffer);
    if (data_size == 0) {
        return add_read_request(ring, conn);
    }

    // 使用 io_uring_prep_send 替代 io_uring_prep_send_zc
    size_t read_index = atomic_load(&conn->write_buffer.read_index) % conn->write_buffer.capacity;
    char* buf = &conn->write_buffer.buffer[read_index];

    io_uring_prep_send(sqe, conn->fd, buf, data_size, 0);
    io_uring_sqe_set_data(sqe, conn);
    conn->state = CONN_STATE_WRITING;
    return 0;
}

static void handle_client_data(ResourceManager *rm, struct connection *conn, ssize_t bytes_read) {
    if (bytes_read <= 0) {
        close_and_free_connection(rm, conn);
        return;
    }

    if (on_data) {
        on_data(&conn->addr, bufs[conn->buffer_id].iov_base, bytes_read);
        if (ring_buffer_write(&conn->write_buffer, bufs[conn->buffer_id].iov_base, bytes_read) != 0) {
            fprintf(stderr, "Failed to write data to buffer\n");
            close_and_free_connection(rm, conn);
            return;
        }
    }

    if (add_write_request(rm->ring, conn) != 0) {
        fprintf(stderr, "Failed to add write request\n");
        close_and_free_connection(rm, conn);
    }
}

static void handle_client_io(ResourceManager *rm, struct io_uring_cqe *cqe) {
    struct connection *conn = (struct connection *)io_uring_cqe_get_data(cqe);
    if (!conn) {
        handle_error(ERR_INVALID_ARGUMENT, "Invalid connection data in completion event");
        return;
    }

    if (cqe->res <= 0) {
        if (cqe->res < 0) {
            fprintf(stderr, "Client IO error: %s\n", strerror(-cqe->res));
        }
        close_and_free_connection(rm, conn);
        return;
    }

    if (conn->state == CONN_STATE_READING) {
        handle_client_data(rm, conn, cqe->res);
    } else if (conn->state == CONN_STATE_WRITING) {
        atomic_fetch_add(&conn->write_buffer.read_index, cqe->res);
        add_read_request(rm->ring, conn);
    }
}

static void handle_accept(ResourceManager *rm, struct io_uring_cqe *cqe) {
    int client_socket = cqe->res;
    if (client_socket < 0) {
        fprintf(stderr, "Accept failed: %s\n", strerror(-client_socket));
        return;
    }

    if (client_socket >= rm->max_connections) {
        handle_error(ERR_CONNECTION_LIMIT_REACHED, "Client socket is out of range");
        close(client_socket);
        return;
    }

    if (set_non_blocking(client_socket) < 0) {
        handle_error(ERR_SOCKET_CREATE_FAILED, "Failed to set client socket to non-blocking");
        close(client_socket);
        return;
    }

    struct connection *conn = create_connection(rm, client_socket);
    if (!conn) {
        close(client_socket);
        return;
    }

    socklen_t addr_len = sizeof(conn->addr);
    getpeername(client_socket, (struct sockaddr*)&conn->addr, &addr_len);

    rm->connections[client_socket] = conn;

    if (on_connect) {
        on_connect(&conn->addr);
    }

    add_read_request(rm->ring, conn);
    add_accept_request(rm->ring, rm->server_socket);
}

static void handle_completion_event(ResourceManager *rm, struct io_uring_cqe *cqe) {
    void *user_data = io_uring_cqe_get_data(cqe);
    if (user_data == (void*)(intptr_t)-1) {
        handle_accept(rm, cqe);
    } else {
        handle_client_io(rm, cqe);
    }
}

void graceful_shutdown(void) {
    keep_running = 0;
}

int start_server(int port) {
    printf("Starting server on port %d\n", port);

    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        handle_error(ERR_RESOURCE_INIT_FAILED, "Failed to set up signal handler");
        return 1;
    }

    struct rlimit rl;
    int max_connections;
    if (getrlimit(RLIMIT_NOFILE, &rl) == 0) {
        printf("Current file descriptor limit: %llu\n", (unsigned long long)rl.rlim_cur);
        max_connections = rl.rlim_cur > 1000 ? rl.rlim_cur - 1000 : rl.rlim_cur / 2;
        printf("Setting max connections to: %d\n", max_connections);
    } else {
        handle_error(ERR_RESOURCE_INIT_FAILED, "Unable to get file descriptor limit");
        max_connections = 1000;
        printf("Unable to get file descriptor limit. Setting max connections to: %d\n", max_connections);
    }

    ResourceManager rm;
    init_resource_manager(&rm, port, max_connections);

    if (allocate_resource(&rm, RESOURCE_SERVER_SOCKET) < 0 ||
        allocate_resource(&rm, RESOURCE_IO_URING) < 0 ||
        allocate_resource(&rm, RESOURCE_CONNECTION_POOL) < 0 ||
        allocate_resource(&rm, RESOURCE_CONNECTIONS_ARRAY) < 0) {
        cleanup_resource_manager(&rm);
        return 1;
    }

    if (setup_buffers(rm.ring) < 0) {
        cleanup_resource_manager(&rm);
        return 1;
    }

    if (add_accept_request(rm.ring, rm.server_socket) < 0) {
        handle_error(ERR_RESOURCE_INIT_FAILED, "Failed to add initial accept request");
        cleanup_resource_manager(&rm);
        return 1;
    }

    printf("Server started. Press Ctrl+C to stop.\n");

    while (keep_running) {
        io_uring_submit(rm.ring);

        struct io_uring_cqe *cqe;
        int ret = io_uring_wait_cqe(rm.ring, &cqe);

        if (ret < 0) {
            if (ret == -EINTR) {
                continue;
            }
            handle_error(ERR_URING_INIT_FAILED, "io_uring_wait_cqe failed");
            break;
        }

        handle_completion_event(&rm, cqe);
        io_uring_cqe_seen(rm.ring, cqe);

        if (!keep_running) {
            break;
        }
    }

    printf("Shutting down server...\n");
    cleanup_resource_manager(&rm);
    free(bufs);
    free(buf_base);
    return 0;
}