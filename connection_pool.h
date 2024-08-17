#ifndef CONNECTION_POOL_H
#define CONNECTION_POOL_H

#include "iouring_server.h"
#include <pthread.h>

typedef struct {
    struct connection **connections;
    int count;
    pthread_mutex_t mutex;
} ConnectionPool;

void init_pool();
struct connection* get_connection();
void put_connection(struct connection* conn);
void clean_pool();

#endif // CONNECTION_POOL_H