#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H

#include <stddef.h>

// 内存池类型
typedef struct MemoryPool MemoryPool;

// 创建内存池
MemoryPool* memory_pool_create(size_t block_size, size_t initial_blocks, size_t alignment);

// 从内存池分配内存
void* memory_pool_alloc(MemoryPool* pool);

// 释放内存回内存池
void memory_pool_free(MemoryPool* pool, void* ptr);

// 销毁内存池
void memory_pool_destroy(MemoryPool* pool);

#endif // MEMORY_POOL_H