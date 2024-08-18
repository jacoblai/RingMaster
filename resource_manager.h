#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include <liburing.h>
#include "memory_pool.h"
#include "iouring_server.h"

// 资源类型枚举
typedef enum {
    RESOURCE_SERVER_SOCKET,
    RESOURCE_IO_URING,
    RESOURCE_CONNECTION_POOL,
    RESOURCE_CONNECTIONS_ARRAY
} ResourceType;

// 资源管理器结构体
typedef struct ResourceManager {
    int server_socket;
    struct io_uring* ring;
    MemoryPool* connection_pool;
    struct connection** connections;
    int port;
    int max_connections;
} ResourceManager;

// 初始化资源管理器
void init_resource_manager(ResourceManager* rm, int port, int max_connections);

// 清理资源管理器
void cleanup_resource_manager(ResourceManager* rm);

// 分配资源
int allocate_resource(ResourceManager* rm, ResourceType type);

// 释放资源
void free_resource(ResourceManager* rm, ResourceType type);

#endif // RESOURCE_MANAGER_H