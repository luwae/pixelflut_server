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

#define COMMAND_NONE 0
#define COMMAND_MULTIRECV_NEXT 1
#define COMMAND_MULTIRECV_DONE 2
#define COMMAND_PRINT 3
#define COMMAND_GET 4
#define COMMAND_MULTIRECV 5
#define COMMAND_MULTISEND 6
#define COMMAND_INFO 7
#define COMMAND_FAULTY 8
#define COMMAND_WOULDBLOCK 9
#define COMMAND_CONNECTION_END 10
#define COMMAND_SYS_ERROR 11
/* returns NONE/MULTIRECV_NEXT/MULTIRECV_DONE */
int connection_recv_from_multi(struct connection *c, struct pixel *px);
/* returns NONE/MULTIRECV_NEXT/PRINT/GET/MULTIRECV/MULTISEND/INFO/FAULTY */
int connection_recv_from_buffer(struct connection *c, struct pixel *px);
/* returns MULTIRECV_NEXT/PRINT/GET/MULTIRECV/MULTISEND/INFO/FAULTY/WOULDBLOCK/CONNECTION_END/SYS_ERROR */
int connection_recv(struct connection *c, struct pixel *px);

#endif
