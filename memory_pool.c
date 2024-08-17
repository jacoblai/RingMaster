#include "memory_pool.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))

// 内存块结构
typedef struct MemoryBlock {
    struct MemoryBlock* next;
} MemoryBlock;

// 内存池结构
struct MemoryPool {
    size_t block_size;
    size_t alignment;
    MemoryBlock* free_blocks;
    MemoryBlock* all_blocks;  // 跟踪所有分配的块
    pthread_mutex_t lock;
};

// 将大小对齐到指定的对齐值
static size_t align_size(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

// 创建内存池
MemoryPool* memory_pool_create(size_t block_size, size_t initial_blocks, size_t alignment) {
    MemoryPool* pool = malloc(sizeof(MemoryPool));
    if (!pool) return NULL;

    // 确保对齐值至少为指针大小，并且是2的幂
    alignment = MAX(alignment, sizeof(void*));
    alignment = (alignment & (alignment - 1)) == 0 ? alignment : 1 << (32 - __builtin_clz((unsigned int)alignment - 1));

    pool->block_size = align_size(MAX(block_size, sizeof(MemoryBlock)), alignment);
    pool->alignment = alignment;
    pool->free_blocks = NULL;
    pool->all_blocks = NULL;

    // 初始化互斥锁
    if (pthread_mutex_init(&pool->lock, NULL) != 0) {
        free(pool);
        return NULL;
    }

    // 预分配初始块
    for (size_t i = 0; i < initial_blocks; i++) {
        void* block = aligned_alloc(alignment, pool->block_size);
        if (!block) {
            memory_pool_destroy(pool);
            return NULL;
        }
        MemoryBlock* mb = (MemoryBlock*)block;
        mb->next = pool->free_blocks;
        pool->free_blocks = mb;

        // 添加到 all_blocks 列表
        MemoryBlock* all_mb = (MemoryBlock*)block;
        all_mb->next = pool->all_blocks;
        pool->all_blocks = all_mb;
    }

    return pool;
}

// 从内存池分配一个块
void* memory_pool_alloc(MemoryPool* pool) {
    pthread_mutex_lock(&pool->lock);

    if (!pool->free_blocks) {
        // 如果没有空闲块，分配一个新的
        void* new_block = aligned_alloc(pool->alignment, pool->block_size);
        if (!new_block) {
            pthread_mutex_unlock(&pool->lock);
            return NULL;
        }
        MemoryBlock* mb = (MemoryBlock*)new_block;
        mb->next = pool->all_blocks;  // 添加到 all_blocks 列表
        pool->all_blocks = mb;
        pthread_mutex_unlock(&pool->lock);
        return new_block;
    }

    // 从空闲列表中取出一个块
    MemoryBlock* block = pool->free_blocks;
    pool->free_blocks = block->next;

    pthread_mutex_unlock(&pool->lock);
    return block;
}

// 将一个块归还给内存池
void memory_pool_free(MemoryPool* pool, void* ptr) {
    if (!ptr) return;

    pthread_mutex_lock(&pool->lock);

    // 将块添加到空闲列表的头部
    MemoryBlock* block = (MemoryBlock*)ptr;
    block->next = pool->free_blocks;
    pool->free_blocks = block;

    pthread_mutex_unlock(&pool->lock);
}

// 销毁内存池
void memory_pool_destroy(MemoryPool* pool) {
    pthread_mutex_lock(&pool->lock);

    // 释放所有分配的块
    MemoryBlock* block = pool->all_blocks;
    while (block) {
        MemoryBlock* next = block->next;
        free(block);
        block = next;
    }

    pthread_mutex_unlock(&pool->lock);
    pthread_mutex_destroy(&pool->lock);
    free(pool);
}