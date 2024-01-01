#ifndef PFS_CONNECTION_H
#define PFS_CONNECTION_H

#include <sys/socket.h>
#include <netinet/in.h>

#include "common.h"

#define CONN_BUF_SIZE 1024
struct connection {
    int fd; // fd == -1 means free
    struct sockaddr_in addr;
    int read_pos;
    int write_pos;
    unsigned char buf[CONN_BUF_SIZE];
};

#define GET_SUCCESS 0
#define GET_WOULDBLOCK 1
#define GET_CONNECTION_END 2
int connection_get(struct connection *conn, struct pixel *px);

void connection_close(struct connection *conn);

void connection_print(const struct connection *conn, int id);

#endif
