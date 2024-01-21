#ifndef PFS_CONNECTION_H
#define PFS_CONNECTION_H

#include <sys/socket.h>

void set_nonblocking(int fd);

struct connection_tracker {
    in_addr_t addr;
    unsigned long long start_time;
    unsigned long long end_time;
    unsigned long long num_bytes_received_from_client;
    unsigned long long num_command_print;
    unsigned long long num_command_get;
    unsigned long long num_read_syscalls;
    unsigned long long num_read_syscalls_wouldblock;
    unsigned long long num_bytes_sent_to_client;
    unsigned long long num_pixels_sent_to_client;
    unsigned long long num_write_syscalls;
    unsigned long long num_write_syscalls_wouldblock;
    unsigned long long num_coords_outside_canvas;
};

void connection_tracker_print(const struct connection_tracker *t);

#define CONN_BUF_SIZE 1024
struct connection {
    int fd; // fd == -1 means free
    struct sockaddr_in addr;
    struct connection_tracker tracker;
    int recv_read_pos;
    int recv_write_pos;
    int send_read_pos;
    int send_write_pos;
    unsigned char recvbuf[CONN_BUF_SIZE];
    unsigned char sendbuf[CONN_BUF_SIZE];
};

void connection_init(struct connection *c, int connfd, struct sockaddr_in connaddr, int id);
void connection_close(struct connection *conn);

#define COMMAND_NONE 0 // only used by connection_recv_from_buffer
#define COMMAND_FAULTY 1
#define COMMAND_PRINT 2
#define COMMAND_GET 3
#define COMMAND_WOULDBLOCK 4 // only used by connection_recv
#define COMMAND_CONNECTION_END 5 // only used by connection_recv
int connection_recv_from_buffer(struct connection *conn, struct pixel *px);
int connection_recv(struct connection *conn, struct pixel *px);

void connection_print(const struct connection *conn, int id);

#endif
