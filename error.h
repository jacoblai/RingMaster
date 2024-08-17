#ifndef ERROR_H
#define ERROR_H

// 定义错误代码枚举
typedef enum {
    ERR_NONE = 0,                  // 无错误
    ERR_MEMORY_ALLOC_FAILED,       // 内存分配失败
    ERR_SOCKET_CREATE_FAILED,      // 套接字创建失败
    ERR_SOCKET_BIND_FAILED,        // 套接字绑定失败
    ERR_SOCKET_LISTEN_FAILED,      // 套接字监听失败
    ERR_URING_INIT_FAILED,         // io_uring 初始化失败
    ERR_URING_QUEUE_FULL,          // io_uring 队列已满
    ERR_CONNECTION_LIMIT_REACHED,  // 达到连接数限制
    ERR_INVALID_ARGUMENT,          // 无效参数
    ERR_RESOURCE_INIT_FAILED       // 资源初始化失败
} ErrorCode;

// 错误处理函数声明
void handle_error(ErrorCode code, const char* message);

#endif // ERROR_H