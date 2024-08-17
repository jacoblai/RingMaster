#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include <liburing.h>
#include "memory_pool.h"
#include "iouring_server.h"

typedef enum {
    RESOURCE_SERVER_SOCKET,
    RESOURCE_IO_URING,
    RESOURCE_CONNECTION_POOL,
    RESOURCE_CONNECTIONS_ARRAY
} ResourceType;

typedef struct {
    int server_socket;
    struct io_uring* ring;
    MemoryPool* connection_pool;
    struct connection** connections;
    int port;
    int max_connections;
} ResourceManager;

void init_resource_manager(ResourceManager* rm, int port, int max_connections);
void cleanup_resource_manager(ResourceManager* rm);
int allocate_resource(ResourceManager* rm, ResourceType type);
void free_resource(ResourceManager* rm, ResourceType type);

#endif // RESOURCE_MANAGER_H