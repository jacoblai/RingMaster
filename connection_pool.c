#include "connection_pool.h"
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
        if (conn) {
            conn->read_buffer = malloc(READ_SZ);
            conn->write_buffer = malloc(READ_SZ);
            if (!conn->read_buffer || !conn->write_buffer) {
                free(conn->read_buffer);
                free(conn->write_buffer);
                free(conn);
                return NULL;
            }
        }
    }

    if (conn) {
        conn->fd = -1;
        conn->state = CONN_STATE_READING;
        conn->read_buffer_size = READ_SZ;
        conn->write_buffer_size = READ_SZ;
        conn->bytes_read = 0;
        conn->bytes_to_write = 0;
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
        free(conn->read_buffer);
        free(conn->write_buffer);
        free(conn);
    }
}

void clean_pool() {
    pthread_mutex_lock(&pool.mutex);
    int to_remove = pool.count / 2;
    for (int i = 0; i < to_remove; i++) {
        struct connection* conn = pool.connections[--pool.count];
        free(conn->read_buffer);
        free(conn->write_buffer);
        free(conn);
    }
    pthread_mutex_unlock(&pool.mutex);
}