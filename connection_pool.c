#include "connection_pool.h"
#include "ring_buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define INITIAL_BUFFER_SIZE 1024

MemoryPool* connection_pool = NULL;

void init_connection_pool(size_t initial_size) {
    connection_pool = memory_pool_create(sizeof(struct connection), initial_size, 64);  // 64字节对齐
    if (!connection_pool) {
        fprintf(stderr, "Failed to create connection pool\n");
        exit(1);
    }
}

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

    memset(conn, 0, sizeof(struct connection));
    conn->fd = -1;
    conn->state = CONN_STATE_READING;

    ring_buffer_init(&conn->read_buffer, INITIAL_BUFFER_SIZE);
    if (conn->read_buffer.buffer == NULL) {
        fprintf(stderr, "Failed to initialize read buffer\n");
        memory_pool_free(connection_pool, conn);
        return NULL;
    }

    ring_buffer_init(&conn->write_buffer, INITIAL_BUFFER_SIZE);
    if (conn->write_buffer.buffer == NULL) {
        fprintf(stderr, "Failed to initialize write buffer\n");
        ring_buffer_destroy(&conn->read_buffer);  // Clean up the read buffer
        memory_pool_free(connection_pool, conn);
        return NULL;
    }

    return conn;
}

void put_connection(struct connection* conn) {
    if (!conn) return;
    if (!connection_pool) {
        fprintf(stderr, "Connection pool not initialized\n");
        return;
    }

    if (conn->fd >= 0) {
        close(conn->fd);
    }
    ring_buffer_destroy(&conn->read_buffer);
    ring_buffer_destroy(&conn->write_buffer);

    memset(conn, 0, sizeof(struct connection));

    memory_pool_free(connection_pool, conn);
}

void clean_connection_pool() {
    if (connection_pool) {
        memory_pool_destroy(connection_pool);
        connection_pool = NULL;
    }
}