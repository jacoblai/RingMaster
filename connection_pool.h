#ifndef CONNECTION_POOL_H
#define CONNECTION_POOL_H

#include "iouring_server.h"
#include "memory_pool.h"

// 声明全局连接池
extern MemoryPool* connection_pool;

// 初始化连接池
// initial_size: 连接池的初始大小
void init_connection_pool(size_t initial_size);

// 从连接池获取一个连接
// 返回: 指向新分配的连接结构的指针，如果分配失败则返回 NULL
struct connection* get_connection();

// 将连接归还到连接池
// conn: 要归还的连接结构指针
void put_connection(struct connection* conn);

// 清理连接池，释放所有资源
void clean_connection_pool();

#endif // CONNECTION_POOL_H