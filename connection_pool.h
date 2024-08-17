#ifndef CONNECTION_POOL_H
#define CONNECTION_POOL_H

#include "iouring_server.h"
#include "memory_pool.h"

extern MemoryPool* connection_pool;

void init_connection_pool(size_t initial_size);
struct connection* get_connection();
void put_connection(struct connection* conn);
void clean_connection_pool();

#endif // CONNECTION_POOL_H