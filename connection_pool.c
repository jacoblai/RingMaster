#include "connection_pool.h"
#include "ring_buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define INITIAL_BUFFER_SIZE 1024

// 全局连接池
MemoryPool* connection_pool = NULL;

// 初始化连接池
void init_connection_pool(size_t initial_size) {
    connection_pool = memory_pool_create(sizeof(struct connection), initial_size, 64);  // 64字节对齐
    if (!connection_pool) {
        fprintf(stderr, "Failed to create connection pool\n");
        exit(1);
    }
}

// 从连接池获取一个连接
struct connection* get_connection() {
    if (!connection_pool) {
        fprintf(stderr, "Connection pool not initialized\n");
        return NULL;
    }

    struct connection* conn = memory_pool_alloc(connection_pool);
    if (conn == NULL) {
        fprintf(stderr, "Failed to allocate new connection from pool\n");
        return NULL;
    }

    // 初始化连接结构体
    memset(conn, 0, sizeof(struct connection));
    conn->fd = -1;
    conn->state = CONN_STATE_READING;
    conn->buffer_id = -1;  // 初始化 buffer_id 为 -1

    // 初始化读缓冲区
    ring_buffer_init(&conn->read_buffer, INITIAL_BUFFER_SIZE);
    if (conn->read_buffer.buffer == NULL) {
        fprintf(stderr, "Failed to initialize read buffer\n");
        memory_pool_free(connection_pool, conn);
        return NULL;
    }

    // 初始化写缓冲区
    ring_buffer_init(&conn->write_buffer, INITIAL_BUFFER_SIZE);
    if (conn->write_buffer.buffer == NULL) {
        fprintf(stderr, "Failed to initialize write buffer\n");
        ring_buffer_destroy(&conn->read_buffer);  // 清理读缓冲区
        memory_pool_free(connection_pool, conn);
        return NULL;
    }

    return conn;
}

// 将连接归还到连接池
void put_connection(struct connection* conn) {
    if (!conn) return;
    if (!connection_pool) {
        fprintf(stderr, "Connection pool not initialized\n");
        return;
    }

    // 关闭文件描述符
    if (conn->fd >= 0) {
        close(conn->fd);
    }
    // 销毁缓冲区
    ring_buffer_destroy(&conn->read_buffer);
    ring_buffer_destroy(&conn->write_buffer);

    // 重置连接结构体
    memset(conn, 0, sizeof(struct connection));
    conn->fd = -1;
    conn->buffer_id = -1;  // 重置 buffer_id 为 -1

    // 将连接归还到内存池
    memory_pool_free(connection_pool, conn);
}

// 清理连接池
void clean_connection_pool() {
    if (connection_pool) {
        memory_pool_destroy(connection_pool);
        connection_pool = NULL;
    }
}