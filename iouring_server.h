#ifndef IOURING_SERVER_H
#define IOURING_SERVER_H

#include <netinet/in.h>
#include "ring_buffer.h"

#define MAX_CONNECTIONS 1024

typedef void (*on_connect_cb)(struct sockaddr_in *);
typedef void (*on_disconnect_cb)(struct sockaddr_in *);
typedef void (*on_data_cb)(struct sockaddr_in *, const char *, size_t);

enum connection_state {
    CONN_STATE_READING,
    CONN_STATE_WRITING
};

struct connection {
    int fd;
    struct sockaddr_in addr;
    RingBuffer read_buffer;
    RingBuffer write_buffer;
    enum connection_state state;
};

void set_on_connect(on_connect_cb cb);
void set_on_disconnect(on_disconnect_cb cb);
void set_on_data(on_data_cb cb);

int start_server(int port);

#endif // IOURING_SERVER_H