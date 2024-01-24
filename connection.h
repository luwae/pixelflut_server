#ifndef PFS_CONNECTION_H
#define PFS_CONNECTION_H

#include <sys/socket.h>

struct connection_tracker {
    in_addr_t addr;
    unsigned long long start_time;
    unsigned long long end_time;
};

void set_nonblocking(int fd);

struct rect_iter {
    int xstart;
    int ystart;
    int xstop;
    int ystop;
    int x;
    int y;
};

int rect_iter_done(const struct rect_iter *r);
void rect_iter_advance(struct rect_iter *r);

#define CONN_BUF_SIZE 1024
struct buffer {
    size_t read_pos;
    size_t write_pos;
    unsigned char data[CONN_BUF_SIZE];
};

int buffer_size(const struct buffer *b);
const unsigned char *buffer_read_reserve(struct buffer *b, unsigned long long n);
unsigned char *buffer_write_reserve(struct buffer *b, unsigned long long n);
void buffer_move_front(struct buffer *b);
int buffer_recv_nonblocking(struct buffer *b, int fd);
int buffer_send_nonblocking(struct buffer *b, int fd);

struct connection {
    int fd; // fd == -1 means free
    struct sockaddr_in addr;
    struct connection_tracker tracker;

    struct rect_iter multirecv;
    struct rect_iter multisend;

    struct buffer recvbuf;
    struct buffer sendbuf;
};

void connection_init(struct connection *c, int connfd, struct sockaddr_in connaddr, int id);
void connection_close(struct connection *conn);

#define COMMAND_NONE 0 // only used by connection_recv_from_buffer
#define COMMAND_FAULTY 1
#define COMMAND_PRINT 2
#define COMMAND_MULTIRECV 3
#define COMMAND_MULTIRECV_DONE 4
#define COMMAND_GET 5
#define COMMAND_MULTISEND 6
#define COMMAND_WOULDBLOCK 7 // only used by connection_recv
#define COMMAND_CONNECTION_END 8 // only used by connection_recv
int connection_recv_from_multi(struct connection *conn, struct pixel *px);
int connection_recv_from_buffer(struct connection *conn, struct pixel *px);
int connection_recv(struct connection *conn, struct pixel *px);

void connection_print(const struct connection *conn, int id);

#endif
