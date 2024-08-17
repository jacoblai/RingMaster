#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdatomic.h>
#include <stddef.h>

#define RING_BUFFER_SIZE 4096
#define RING_BUFFER_MASK (RING_BUFFER_SIZE - 1)

typedef struct {
    char buffer[RING_BUFFER_SIZE];
    atomic_size_t read_index;
    atomic_size_t write_index;
} RingBuffer;

void ring_buffer_init(RingBuffer* rb);
size_t ring_buffer_free_space(RingBuffer* rb);
size_t ring_buffer_used_space(RingBuffer* rb);
size_t ring_buffer_write(RingBuffer* rb, const char* data, size_t len);
size_t ring_buffer_read(RingBuffer* rb, char* data, size_t len);

#endif // RING_BUFFER_H