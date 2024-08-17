#include "ring_buffer.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MIN_BUFFER_SIZE 64
#define MAX_BUFFER_SIZE ((size_t)-1 >> 1)  // 最大缓冲区大小为 SIZE_MAX / 2

// 调整环形缓冲区大小
static int ring_buffer_resize(RingBuffer* rb, size_t new_size) {
    if (new_size <= rb->capacity) {
        return -1;  // 新大小必须大于当前容量
    }

    if (new_size > MAX_BUFFER_SIZE) {
        return -1;  // 防止溢出
    }

    char* new_buffer = realloc(rb->buffer, new_size);
    if (new_buffer == NULL) {
        return -1;  // 调整大小失败
    }

    // 如果数据在旧缓冲区中环绕，需要重新排列
    size_t used = ring_buffer_used_space(rb);
    size_t read_index = atomic_load(&rb->read_index) % rb->capacity;
    size_t write_index = atomic_load(&rb->write_index) % rb->capacity;

    if (write_index < read_index) {
        // 数据环绕，需要使其连续
        memmove(new_buffer + rb->capacity, new_buffer, write_index);
        atomic_store(&rb->write_index, read_index + used);
    }

    rb->buffer = new_buffer;
    rb->capacity = new_size;
    atomic_store(&rb->read_index, read_index);

    return 0;
}

// 初始化环形缓冲区
void ring_buffer_init(RingBuffer* rb, size_t initial_size) {
    if (initial_size < MIN_BUFFER_SIZE) {
        initial_size = MIN_BUFFER_SIZE;
    }
    if (initial_size > MAX_BUFFER_SIZE) {
        initial_size = MAX_BUFFER_SIZE;
    }

    rb->buffer = malloc(initial_size);
    if (rb->buffer == NULL) {
        // 处理分配失败
        rb->capacity = 0;
        atomic_init(&rb->read_index, 0);
        atomic_init(&rb->write_index, 0);
        return;
    }

    rb->capacity = initial_size;
    atomic_init(&rb->read_index, 0);
    atomic_init(&rb->write_index, 0);

    // 初始化互斥锁
    if (pthread_mutex_init(&rb->mutex, NULL) != 0) {
        // 处理互斥锁初始化失败
        free(rb->buffer);
        rb->buffer = NULL;
        rb->capacity = 0;
        return;
    }
}

// 销毁环形缓冲区
void ring_buffer_destroy(RingBuffer* rb) {
    if (rb->buffer != NULL) {
        free(rb->buffer);
        rb->buffer = NULL;
        rb->capacity = 0;
        atomic_store(&rb->read_index, 0);
        atomic_store(&rb->write_index, 0);

        // 销毁互斥锁
        pthread_mutex_destroy(&rb->mutex);
    }
}

// 获取环形缓冲区中的可用空间
size_t ring_buffer_free_space(const RingBuffer* rb) {
    return rb->capacity - ring_buffer_used_space(rb);
}

// 获取环形缓冲区中已使用的空间
size_t ring_buffer_used_space(const RingBuffer* rb) {
    size_t write_index = atomic_load_explicit(&rb->write_index, memory_order_acquire);
    size_t read_index = atomic_load_explicit(&rb->read_index, memory_order_acquire);
    return (write_index >= read_index) ? (write_index - read_index) :
           (SIZE_MAX - read_index + write_index + 1);
}

// 写入数据到环形缓冲区
int ring_buffer_write(RingBuffer* rb, const char* data, size_t len) {
    pthread_mutex_lock(&rb->mutex);

    if (ring_buffer_free_space(rb) < len) {
        size_t new_size = rb->capacity;
        size_t required_size = ring_buffer_used_space(rb) + len;
        while (new_size < required_size) {
            if (new_size > MAX_BUFFER_SIZE / 2) {
                pthread_mutex_unlock(&rb->mutex);
                return -1;  // 防止溢出
            }
            new_size = new_size * 3 / 2;  // 每次增长50%
        }
        if (ring_buffer_resize(rb, new_size) != 0) {
            pthread_mutex_unlock(&rb->mutex);
            return -1;  // 调整大小失败
        }
    }

    size_t write_index = atomic_load_explicit(&rb->write_index, memory_order_relaxed) % rb->capacity;
    size_t end = (write_index + len) % rb->capacity;

    if (end > write_index) {
        memcpy(rb->buffer + write_index, data, len);
    } else {
        size_t first_part = rb->capacity - write_index;
        memcpy(rb->buffer + write_index, data, first_part);
        memcpy(rb->buffer, data + first_part, len - first_part);
    }

    atomic_fetch_add_explicit(&rb->write_index, len, memory_order_release);

    pthread_mutex_unlock(&rb->mutex);
    return 0;
}

// 从环形缓冲区读取数据
size_t ring_buffer_read(RingBuffer* rb, char* data, size_t len) {
    size_t available = ring_buffer_used_space(rb);
    size_t read_size = (len < available) ? len : available;

    if (read_size == 0) {
        return 0;
    }

    pthread_mutex_lock(&rb->mutex);

    size_t read_index = atomic_load_explicit(&rb->read_index, memory_order_relaxed) % rb->capacity;
    size_t end = (read_index + read_size) % rb->capacity;

    if (end > read_index) {
        memcpy(data, rb->buffer + read_index, read_size);
    } else {
        size_t first_part = rb->capacity - read_index;
        memcpy(data, rb->buffer + read_index, first_part);
        memcpy(data + first_part, rb->buffer, read_size - first_part);
    }

    atomic_fetch_add_explicit(&rb->read_index, read_size, memory_order_release);

    // 如果我们读取了所有数据，重置索引以避免溢出
    if (atomic_load_explicit(&rb->read_index, memory_order_acquire) ==
        atomic_load_explicit(&rb->write_index, memory_order_acquire)) {
        atomic_store_explicit(&rb->read_index, 0, memory_order_relaxed);
        atomic_store_explicit(&rb->write_index, 0, memory_order_relaxed);
    }

    pthread_mutex_unlock(&rb->mutex);
    return read_size;
}

// 查看环形缓冲区中的数据而不移除
int ring_buffer_peek(const RingBuffer* rb, char* data, size_t len) {
    size_t available = ring_buffer_used_space(rb);
    size_t peek_size = (len < available) ? len : available;

    if (peek_size == 0) {
        return 0;
    }

    size_t read_index = atomic_load(&rb->read_index) % rb->capacity;
    size_t end = (read_index + peek_size) % rb->capacity;

    if (end > read_index) {
        memcpy(data, rb->buffer + read_index, peek_size);
    } else {
        size_t first_part = rb->capacity - read_index;
        memcpy(data, rb->buffer + read_index, first_part);
        memcpy(data + first_part, rb->buffer, peek_size - first_part);
    }

    return peek_size;
}