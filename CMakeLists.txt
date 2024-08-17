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
        connection_pool.c
)

# 链接 liburing 和 pthread
target_link_libraries(iouring_server ${URING_LIBRARY} pthread)