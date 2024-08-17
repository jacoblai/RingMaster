#include "connection_pool.h"
#include "ring_buffer.h"
#include <stdlib.h>
#include <string.h>

static ConnectionPool pool = {0};

void init_pool() {
    pthread_mutex_init(&pool.mutex, NULL);
}

struct connection* get_connection() {
    struct connection* conn = NULL;

    pthread_mutex_lock(&pool.mutex);
    if (pool.count > 0) {
        conn = pool.connections[--pool.count];
    }
    pthread_mutex_unlock(&pool.mutex);

    if (conn == NULL) {
        conn = malloc(sizeof(struct connection));
    }

    if (conn) {
        conn->fd = -1;
        conn->state = CONN_STATE_READING;
        ring_buffer_init(&conn->read_buffer);
        ring_buffer_init(&conn->write_buffer);
    }

    return conn;
}

void put_connection(struct connection* conn) {
    if (!conn) return;

    pthread_mutex_lock(&pool.mutex);
    if (pool.count < POOL_MAX_SIZE) {
        pool.connections[pool.count++] = conn;
        pthread_mutex_unlock(&pool.mutex);
    } else {
        pthread_mutex_unlock(&pool.mutex);
        // 直接释放整个 connection 结构体
        free(conn);
    }
}

void clean_pool() {
    pthread_mutex_lock(&pool.mutex);
    int to_remove = pool.count / 2;
    for (int i = 0; i < to_remove; i++) {
        struct connection* conn = pool.connections[--pool.count];
        // 不需要释放 read_buffer 和 write_buffer，因为它们是结构体的一部分
        free(conn);
    }
    pthread_mutex_unlock(&pool.mutex);
}