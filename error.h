#ifndef ERROR_H
#define ERROR_H

typedef enum {
    ERR_NONE = 0,
    ERR_MEMORY_ALLOC_FAILED,
    ERR_SOCKET_CREATE_FAILED,
    ERR_SOCKET_BIND_FAILED,
    ERR_SOCKET_LISTEN_FAILED,
    ERR_URING_INIT_FAILED,
    ERR_URING_QUEUE_FULL,
    ERR_CONNECTION_LIMIT_REACHED,
    ERR_INVALID_ARGUMENT,
    ERR_RESOURCE_INIT_FAILED
} ErrorCode;

void handle_error(ErrorCode code, const char* message);

#endif // ERROR_H