#include "iouring_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <ctype.h>

// 新连接建立时的回调函数
void on_connect_handler(struct sockaddr_in *addr) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr->sin_addr), ip, INET_ADDRSTRLEN);
    // printf("New connection from %s:%d\n", ip, ntohs(addr->sin_port));
}

// 连接断开时的回调函数
void on_disconnect_handler(struct sockaddr_in *addr) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr->sin_addr), ip, INET_ADDRSTRLEN);
    // printf("Disconnected: %s:%d\n", ip, ntohs(addr->sin_port));
}

// 接收到数据时的回调函数
void on_data_handler(struct connection* conn, const char *data, size_t len, struct ResourceManager* rm) {
    (void)rm;  // 显式忽略 rm 参数，消除未使用参数的警告

    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(conn->addr.sin_addr), ip, INET_ADDRSTRLEN);

    // printf("Received %zu bytes from %s:%d\n", len, ip, ntohs(conn->addr.sin_port));

    // Echo the data back
    if (ring_buffer_write(&conn->write_buffer, data, len) != 0) {
        fprintf(stderr, "Failed to write data to buffer for echoing\n");
    }
}

int main(int argc, char *argv[]) {
    // 检查命令行参数
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    // 解析端口号
    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number\n");
        return 1;
    }

    // 设置回调函数
    set_on_connect(on_connect_handler);
    set_on_disconnect(on_disconnect_handler);
    set_on_data(on_data_handler);

    // 启动服务器
    return start_server(port);
}