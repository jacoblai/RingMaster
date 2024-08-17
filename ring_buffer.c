#include "ring_buffer.h"
#include <string.h>

void ring_buffer_init(RingBuffer* rb) {
    atomic_init(&rb->read_index, 0);
    atomic_init(&rb->write_index, 0);
}

size_t ring_buffer_free_space(RingBuffer* rb) {
    return RING_BUFFER_SIZE - (atomic_load(&rb->write_index) - atomic_load(&rb->read_index));
}

size_t ring_buffer_used_space(RingBuffer* rb) {
    return atomic_load(&rb->write_index) - atomic_load(&rb->read_index);
}

size_t ring_buffer_write(RingBuffer* rb, const char* data, size_t len) {
    size_t available = ring_buffer_free_space(rb);
    size_t write_size = (len < available) ? len : available;
    size_t write_index = atomic_load(&rb->write_index);

    for (size_t i = 0; i < write_size; i++) {
        rb->buffer[(write_index + i) & RING_BUFFER_MASK] = data[i];
    }

    atomic_fetch_add(&rb->write_index, write_size);
    return write_size;
}

size_t ring_buffer_read(RingBuffer* rb, char* data, size_t len) {
    size_t available = ring_buffer_used_space(rb);
    size_t read_size = (len < available) ? len : available;
    size_t read_index = atomic_load(&rb->read_index);

    for (size_t i = 0; i < read_size; i++) {
        data[i] = rb->buffer[(read_index + i) & RING_BUFFER_MASK];
    }

    atomic_fetch_add(&rb->read_index, read_size);
    return read_size;
}