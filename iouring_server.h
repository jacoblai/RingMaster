#ifndef IOURING_SERVER_H
#define IOURING_SERVER_H

#include <netinet/in.h>

#define MAX_CONNECTIONS 1024
#define BUFFER_SIZE 8192

typedef void (*on_connect_cb)(struct sockaddr_in *);
typedef void (*on_disconnect_cb)(struct sockaddr_in *);
typedef void (*on_data_cb)(struct sockaddr_in *, const char *, size_t);

void set_on_connect(on_connect_cb cb);
void set_on_disconnect(on_disconnect_cb cb);
void set_on_data(on_data_cb cb);

int start_server(int port);

#endif // IOURING_SERVER_H