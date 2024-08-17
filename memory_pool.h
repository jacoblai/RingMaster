#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <stddef.h>

typedef struct MemoryPool MemoryPool;

MemoryPool* memory_pool_create(size_t block_size, size_t initial_blocks);
void* memory_pool_alloc(MemoryPool* pool);
void memory_pool_free(MemoryPool* pool, void* ptr);
void memory_pool_destroy(MemoryPool* pool);

#endif // MEMORY_POOL_H