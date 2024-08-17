#include "memory_pool.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>

#define MAX(a, b) ((a) > (b) ? (a) : (b))

typedef struct MemoryBlock {
    struct MemoryBlock* next;
} MemoryBlock;

struct MemoryPool {
    size_t block_size;
    size_t alignment;
    MemoryBlock* free_blocks;
    pthread_mutex_t lock;
};

static size_t align_size(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

MemoryPool* memory_pool_create(size_t block_size, size_t initial_blocks, size_t alignment) {
    MemoryPool* pool = malloc(sizeof(MemoryPool));
    if (!pool) return NULL;

    alignment = MAX(alignment, sizeof(void*));
    alignment = (alignment & (alignment - 1)) == 0 ? alignment : 1 << (32 - __builtin_clz((unsigned int)alignment - 1));

    pool->block_size = align_size(MAX(block_size, sizeof(MemoryBlock)), alignment);
    pool->alignment = alignment;
    pool->free_blocks = NULL;

    if (pthread_mutex_init(&pool->lock, NULL) != 0) {
        free(pool);
        return NULL;
    }

    for (size_t i = 0; i < initial_blocks; i++) {
        void* block = aligned_alloc(alignment, pool->block_size);
        if (!block) {
            memory_pool_destroy(pool);
            return NULL;
        }
        MemoryBlock* mb = (MemoryBlock*)block;
        mb->next = pool->free_blocks;
        pool->free_blocks = mb;
    }

    return pool;
}

void* memory_pool_alloc(MemoryPool* pool) {
    pthread_mutex_lock(&pool->lock);

    if (!pool->free_blocks) {
        void* new_block = aligned_alloc(pool->alignment, pool->block_size);
        if (!new_block) {
            pthread_mutex_unlock(&pool->lock);
            return NULL;
        }
        pthread_mutex_unlock(&pool->lock);
        return new_block;
    }

    MemoryBlock* block = pool->free_blocks;
    pool->free_blocks = block->next;

    pthread_mutex_unlock(&pool->lock);
    return block;
}

void memory_pool_free(MemoryPool* pool, void* ptr) {
    if (!ptr) return;

    pthread_mutex_lock(&pool->lock);

    MemoryBlock* block = (MemoryBlock*)ptr;
    block->next = pool->free_blocks;
    pool->free_blocks = block;

    pthread_mutex_unlock(&pool->lock);
}

void memory_pool_destroy(MemoryPool* pool) {
    pthread_mutex_lock(&pool->lock);

    MemoryBlock* block = pool->free_blocks;
    while (block) {
        MemoryBlock* next = block->next;
        free(block);
        block = next;
    }

    pthread_mutex_unlock(&pool->lock);
    pthread_mutex_destroy(&pool->lock);
    free(pool);
}