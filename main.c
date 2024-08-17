#include <ctype.h>

#include "iouring_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

void on_connect_handler(struct sockaddr_in *addr) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr->sin_addr), ip, INET_ADDRSTRLEN);
    printf("New connection from %s:%d\n", ip, ntohs(addr->sin_port));
}

void on_disconnect_handler(struct sockaddr_in *addr) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr->sin_addr), ip, INET_ADDRSTRLEN);
    printf("Disconnected: %s:%d\n", ip, ntohs(addr->sin_port));
}

void on_data_handler(struct sockaddr_in *addr, const char *data, size_t len) {
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(addr->sin_addr), ip, INET_ADDRSTRLEN);
    printf("Received %zu bytes from %s:%d\n", len, ip, ntohs(addr->sin_port));
    printf("Data: ");
    for (size_t i = 0; i < len && i < 100; i++) {  // 限制打印的数据量
        if (isprint(data[i])) {
            putchar(data[i]);
        } else {
            printf("\\x%02x", (unsigned char)data[i]);
        }
    }
    if (len > 100) printf("...");
    printf("\n");
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        fprintf(stderr, "Invalid port number\n");
        return 1;
    }

    set_on_connect(on_connect_handler);
    set_on_disconnect(on_disconnect_handler);
    set_on_data(on_data_handler);

    return start_server(port);
}