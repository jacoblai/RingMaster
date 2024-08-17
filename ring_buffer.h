#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdatomic.h>
#include <stddef.h>
#include <pthread.h>

// 环形缓冲区结构体
typedef struct {
    char *buffer;
    size_t capacity;
    atomic_size_t read_index;
    atomic_size_t write_index;
    pthread_mutex_t mutex;
} RingBuffer;

// 初始化环形缓冲区
void ring_buffer_init(RingBuffer* rb, size_t initial_size);

// 销毁环形缓冲区
void ring_buffer_destroy(RingBuffer* rb);

// 获取环形缓冲区中的可用空间
size_t ring_buffer_free_space(const RingBuffer* rb);

// 获取环形缓冲区中已使用的空间
size_t ring_buffer_used_space(const RingBuffer* rb);

// 写入数据到环形缓冲区
int ring_buffer_write(RingBuffer* rb, const char* data, size_t len);

// 从环形缓冲区读取数据
size_t ring_buffer_read(RingBuffer* rb, char* data, size_t len);

// 查看环形缓冲区中的数据而不移除
int ring_buffer_peek(const RingBuffer* rb, char* data, size_t len);

#endif // RING_BUFFER_H