#ifndef IOURING_SERVER_H
#define IOURING_SERVER_H

#include <netinet/in.h>
#include "ring_buffer.h"
#include <liburing.h>

#define MAX_CONNECTIONS 1000000
#define QUEUE_DEPTH 32768
#define BUFFER_SIZE 1024
#define BUFFER_COUNT 5000

// 前向声明
struct connection;
struct ResourceManager;

// 连接状态枚举
enum connection_state {
    CONN_STATE_READING,
    CONN_STATE_WRITING
};

// 连接结构体
struct connection {
    int fd;
    struct sockaddr_in addr;
    RingBuffer read_buffer;
    RingBuffer write_buffer;
    enum connection_state state;
    int buffer_id;  // 用于零拷贝操作的缓冲区ID
};

// 回调函数类型定义
typedef void (*on_connect_cb)(struct sockaddr_in *);
typedef void (*on_disconnect_cb)(struct sockaddr_in *);
typedef void (*on_data_cb)(struct connection*, const char*, size_t, struct ResourceManager*);

// 设置回调函数
void set_on_connect(on_connect_cb cb);
void set_on_disconnect(on_disconnect_cb cb);
void set_on_data(on_data_cb cb);

// 启动服务器
int start_server(int port);

// 优雅关闭服务器
void graceful_shutdown(void);

#endif // IOURING_SERVER_H