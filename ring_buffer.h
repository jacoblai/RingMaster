#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdatomic.h>
#include <stddef.h>
#include <pthread.h>

typedef struct {
    char *buffer;
    size_t capacity;
    atomic_size_t read_index;
    atomic_size_t write_index;
    pthread_mutex_t mutex;
} RingBuffer;

void ring_buffer_init(RingBuffer* rb, size_t initial_size);
void ring_buffer_destroy(RingBuffer* rb);
size_t ring_buffer_free_space(const RingBuffer* rb);
size_t ring_buffer_used_space(const RingBuffer* rb);
int ring_buffer_write(RingBuffer* rb, const char* data, size_t len);
size_t ring_buffer_read(RingBuffer* rb, char* data, size_t len);
int ring_buffer_peek(const RingBuffer* rb, char* data, size_t len);

#endif // RING_BUFFER_H