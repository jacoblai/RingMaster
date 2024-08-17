#include "resource_manager.h"
#include "error.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

static int setup_listening_socket(int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        handle_error(ERR_SOCKET_CREATE_FAILED, "Failed to create socket");
        return -1;
    }

    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        handle_error(ERR_SOCKET_CREATE_FAILED, "setsockopt(SO_REUSEADDR) failed");
        close(sock);
        return -1;
    }

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        handle_error(ERR_SOCKET_BIND_FAILED, "Failed to bind to port");
        close(sock);
        return -1;
    }

    if (listen(sock, SOMAXCONN) < 0) {
        handle_error(ERR_SOCKET_LISTEN_FAILED, "Failed to listen on socket");
        close(sock);
        return -1;
    }

    return sock;
}

void init_resource_manager(ResourceManager* rm, int port, int max_connections) {
    rm->server_socket = -1;
    rm->ring = NULL;
    rm->connection_pool = NULL;
    rm->connections = NULL;
    rm->port = port;
    rm->max_connections = max_connections;
}

void cleanup_resource_manager(ResourceManager* rm) {
    if (rm->server_socket >= 0) {
        close(rm->server_socket);
    }
    if (rm->ring) {
        io_uring_queue_exit(rm->ring);
        free(rm->ring);
    }
    if (rm->connection_pool) {
        memory_pool_destroy(rm->connection_pool);
    }
    if (rm->connections) {
        free(rm->connections);
    }
}

int allocate_resource(ResourceManager* rm, ResourceType type) {
    switch (type) {
        case RESOURCE_SERVER_SOCKET:
            rm->server_socket = setup_listening_socket(rm->port);
            if (rm->server_socket < 0) {
                return -1;
            }
            break;

        case RESOURCE_IO_URING:
            rm->ring = malloc(sizeof(struct io_uring));
            if (!rm->ring) {
                handle_error(ERR_MEMORY_ALLOC_FAILED, "Failed to allocate memory for io_uring");
                return -1;
            }
            if (io_uring_queue_init(QUEUE_DEPTH, rm->ring, 0) < 0) {
                handle_error(ERR_URING_INIT_FAILED, "Failed to initialize io_uring");
                free(rm->ring);
                rm->ring = NULL;
                return -1;
            }
            break;

        case RESOURCE_CONNECTION_POOL:
            rm->connection_pool = memory_pool_create(sizeof(struct connection), 1000, 64);
            if (!rm->connection_pool) {
                handle_error(ERR_MEMORY_ALLOC_FAILED, "Failed to create connection pool");
                return -1;
            }
            break;

        case RESOURCE_CONNECTIONS_ARRAY:
            rm->connections = calloc(rm->max_connections, sizeof(struct connection*));
            if (!rm->connections) {
                handle_error(ERR_MEMORY_ALLOC_FAILED, "Failed to allocate connections array");
                return -1;
            }
            break;

        default:
            handle_error(ERR_INVALID_ARGUMENT, "Invalid resource type");
            return -1;
    }

    return 0;
}

void free_resource(ResourceManager* rm, ResourceType type) {
    switch (type) {
        case RESOURCE_SERVER_SOCKET:
            if (rm->server_socket >= 0) {
                close(rm->server_socket);
                rm->server_socket = -1;
            }
            break;

        case RESOURCE_IO_URING:
            if (rm->ring) {
                io_uring_queue_exit(rm->ring);
                free(rm->ring);
                rm->ring = NULL;
            }
            break;

        case RESOURCE_CONNECTION_POOL:
            if (rm->connection_pool) {
                memory_pool_destroy(rm->connection_pool);
                rm->connection_pool = NULL;
            }
            break;

        case RESOURCE_CONNECTIONS_ARRAY:
            if (rm->connections) {
                free(rm->connections);
                rm->connections = NULL;
            }
            break;

        default:
            handle_error(ERR_INVALID_ARGUMENT, "Invalid resource type");
            break;
    }
}