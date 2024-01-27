#ifndef PFS_CONNECTION_H
#define PFS_CONNECTION_H

#include <sys/socket.h>
#include "buffer.h"

void set_nonblocking(int fd);

struct connection_tracker {
    in_addr_t addr;
    unsigned long long start_time;
    unsigned long long end_time;
};

void connection_tracker_init(struct connection_tracker *t, in_addr_t addr, unsigned long long start_time);
void connection_tracker_print(const struct connection_tracker *t);

struct rect_iter {
    int xstart;
    int ystart;
    int xstop;
    int ystop;
    int x;
    int y;
};

void rect_iter_init(struct rect_iter *r);
int rect_iter_done(const struct rect_iter *r);
void rect_iter_advance(struct rect_iter *r);

struct connection {
    int fd; // fd == -1 means free
    struct sockaddr_in addr;
    struct connection_tracker tracker;
    struct rect_iter multirecv;
    struct rect_iter multisend;
    struct buffer recvbuf;
    struct buffer sendbuf;
};

void connection_print(const struct connection *c);
void connection_init(struct connection *c, int connfd, struct sockaddr_in connaddr);
void connection_close(struct connection *c);

#define CONNECTION_OK 0
#define CONNECTION_ERR 1
#define CONNECTION_END 2
int connection_step(struct connection *c);

#endif
