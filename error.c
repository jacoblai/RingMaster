#include "error.h"
#include <stdio.h>
#include <stdlib.h>

// 处理错误的函数
void handle_error(ErrorCode code, const char* message) {
    // 输出错误信息到标准错误流
    fprintf(stderr, "Error: %s (Code: %d)\n", message, code);

    // 根据错误代码决定处理方式
    switch (code) {
        case ERR_MEMORY_ALLOC_FAILED:
        case ERR_SOCKET_CREATE_FAILED:
        case ERR_URING_INIT_FAILED:
        case ERR_RESOURCE_INIT_FAILED:
            // 这些错误被认为是致命错误，程序将退出
            exit(EXIT_FAILURE);
        default:
            // 对于其他错误，让调用者决定如何处理
                break;
    }
}