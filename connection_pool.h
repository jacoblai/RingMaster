#ifndef CONNECTION_POOL_H
#define CONNECTION_POOL_H

#include "iouring_server.h"
#include <pthread.h>

#define POOL_MAX_SIZE 1000

typedef struct {
    struct connection *connections[POOL_MAX_SIZE];
    int count;
    pthread_mutex_t mutex;
} ConnectionPool;

void init_pool();
struct connection* get_connection();
void put_connection(struct connection* conn);
void clean_pool();

#endif // CONNECTION_POOL_H