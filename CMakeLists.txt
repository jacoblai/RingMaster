cmake_minimum_required(VERSION 3.10)
project(iouring_server C)

set(CMAKE_C_STANDARD 11)

# 添加编译标志
add_compile_options(-Wall -Wextra -g)

# 查找 liburing
find_library(URING_LIBRARY uring)

# 添加可执行文件
add_executable(iouring_server
        main.c
        iouring_server.c
        iouring_server.h
        connection_pool.c
        connection_pool.h
        ring_buffer.c
        ring_buffer.h
        memory_pool.c
        memory_pool.h
        error.c
        error.h
        resource_manager.c
        resource_manager.h
)

# 链接 liburing 和 pthread
target_link_libraries(iouring_server ${URING_LIBRARY} pthread)