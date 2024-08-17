#include "memory_pool.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

typedef struct MemoryBlock {
    struct MemoryBlock* next;
} MemoryBlock;

struct MemoryPool {
    size_t block_size;
    MemoryBlock* free_blocks;
    pthread_mutex_t lock;
};

MemoryPool* memory_pool_create(size_t block_size, size_t initial_blocks) {
    MemoryPool* pool = malloc(sizeof(MemoryPool));
    if (!pool) return NULL;

    pool->block_size = block_size > sizeof(MemoryBlock) ? block_size : sizeof(MemoryBlock);
    pool->free_blocks = NULL;

    if (pthread_mutex_init(&pool->lock, NULL) != 0) {
        free(pool);
        return NULL;
    }

    for (size_t i = 0; i < initial_blocks; i++) {
        MemoryBlock* block = malloc(pool->block_size);
        if (!block) {
            memory_pool_destroy(pool);
            return NULL;
        }
        block->next = pool->free_blocks;
        pool->free_blocks = block;
    }

    return pool;
}

void* memory_pool_alloc(MemoryPool* pool) {
    pthread_mutex_lock(&pool->lock);

    if (!pool->free_blocks) {
        MemoryBlock* new_block = malloc(pool->block_size);
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

    MemoryBlock* block = ptr;
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