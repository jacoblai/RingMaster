#include "error.h"
#include <stdio.h>
#include <stdlib.h>

void handle_error(ErrorCode code, const char* message) {
    fprintf(stderr, "Error: %s (Code: %d)\n", message, code);

    switch (code) {
        case ERR_MEMORY_ALLOC_FAILED:
        case ERR_SOCKET_CREATE_FAILED:
        case ERR_URING_INIT_FAILED:
        case ERR_RESOURCE_INIT_FAILED:
            exit(EXIT_FAILURE);
        default:
            // For other errors, we'll let the caller decide what to do
                break;
    }
}