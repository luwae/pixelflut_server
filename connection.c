#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include "SDL.h"

#include "common.h"
#include "canvas.h"

/*
#define NUM_TRACKERS 1024
struct connection_tracker {
    in_addr_t addr;
    unsigned long long start_time;
    unsigned long long end_time;
    unsigned long long num_bytes_received_from_client;
    unsigned long long num_pixels_received_from_client;
    unsigned long long num_read_syscalls;
    unsigned long long num_read_syscalls_wouldblock;
    unsigned long long num_pixels_sent_to_client;
    unsigned long long num_bytes_sent_to_client;
    unsigned long long num_write_syscalls;
    unsigned long long num_write_syscalls_wouldblock;
    unsigned long long num_coords_outside_canvas;
};
struct connection_tracker trackers[NUM_TRACKERS];
int trackerp = 0;
*/

#define CONN_BUF_SIZE 1024
struct connection {
    int fd; // fd == -1 means free
    struct sockaddr_in addr;
    // struct connection_tracker *tracker;
    int recv_read_pos;
    int recv_write_pos;
    int send_read_pos;
    int send_write_pos;
    unsigned char recvbuf[CONN_BUF_SIZE];
    unsigned char sendbuf[CONN_BUF_SIZE];
};

#define PORT 1337

#define MAX_CONNS 10
struct connection conns[MAX_CONNS];
pthread_t net_thread;
volatile int should_quit = 0;

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl");
        exit(1); // TODO
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl");
        exit(1); // TODO
    }
}

static void connection_close(struct connection *conn) {
    // conn->tracker->end_time = SDL_GetTicks64(); // TODO use OS functionality
    close(conn->fd);
    conn->fd = -1;
}

// TODO incorporate alpha, or do some other funny commands (mix-multiply, mix-add)
// TODO perhaps we don't need alpha, since we have the pixel read functionality
// TODO perhaps 12-bit x and 12-bit y?
// TODO kernel operations?
/* Message (8 byte)
 * 'P'
 * x (lo)
 * x (hi)
 * y (lo)
 * y (hi)
 * r
 * g
 * b
 */

/* Message (8 byte)
 * 'G'
 * x (lo)
 * x (hi)
 * y (lo)
 * y (hi)
 * -- padding --
 * -- padding --
 * -- padding --
 */

/* Answer (4 bytes)
 * r
 * g
 * b
 * inside canvas -> 1; outside canvas -> 0
 */

#define COMMAND_NONE 0 // only used by connection_recv_from_buffer
#define COMMAND_FAULTY 1
#define COMMAND_PRINT 2
#define COMMAND_GET 3
#define COMMAND_WOULDBLOCK 4 // only used by connection_recv
#define COMMAND_CONNECTION_END 5 // only used by connection_recv

static int connection_recv_from_buffer(struct connection *conn, struct pixel *px) {
    if (conn->recv_write_pos - conn->recv_read_pos < 8)
        return COMMAND_NONE;
    unsigned char *rp = &conn->recvbuf[conn->recv_read_pos];
    conn->recv_read_pos += 8; // advance, even if faulty
    if (rp[0] == 'P') {
        px->x = rp[1] | (rp[2] << 8);
        px->y = rp[3] | (rp[4] << 8);
        px->r = rp[5];
        px->g = rp[6];
        px->b = rp[7];
        return COMMAND_PRINT;
    } else if (rp[0] == 'G') {
        px->x = rp[1] | (rp[2] << 8);
        px->y = rp[3] | (rp[4] << 8);
        return COMMAND_GET;
    } else {
        return COMMAND_FAULTY;
    }
}

static int connection_recv(struct connection *conn, struct pixel *px) {
    int status = connection_recv_from_buffer(conn, px); // fast path - we already read enough
    if (status != COMMAND_NONE)
        return status; // on faulty command, client is skipped (does nothing).

    int recvbuf_size = conn->recv_write_pos - conn->recv_read_pos;
    if (conn->recv_read_pos > 0) {
        // there is not enough in the buffer. copy the rest and try reading
        // we can use memcpy because there are less than 8 bytes to be copied
        memcpy(conn->recvbuf, &conn->recvbuf[conn->recv_read_pos], recvbuf_size);
        conn->recv_read_pos = 0;
        conn->recv_write_pos = recvbuf_size;
    }
    status = read(conn->fd, &conn->recvbuf[conn->recv_write_pos], CONN_BUF_SIZE - conn->recv_write_pos);
    // conn->tracker->num_read_syscalls++;
    if (WOULD_BLOCK(status)) {
        // conn->tracker->num_read_syscalls_blocked++;
        return COMMAND_WOULDBLOCK;
    } else if (status == -1) {
        perror("read");
        exit(1); // TODO
    } else if (status == 0) {
        return COMMAND_CONNECTION_END;
    } else {
        conn->recv_write_pos += status;
        // conn->tracker->num_bytes_read += status;
        status = connection_recv_from_buffer(conn, px);
        if (status != COMMAND_NONE) {
            return status;
        } else {
            // we still don't have enough bytes. just interpret this as wouldblock
            // NOTE for later: don't increase num_read_syscalls_wouldblock here
            return COMMAND_WOULDBLOCK;
        }
    }
}

static void connection_print(const struct connection *conn, int id) {
    in_addr_t a = conn->addr.sin_addr.s_addr;
    printf("Connection { ip: %d.%d.%d.%d", a & 0xff, (a >> 8) & 0xff, (a >> 16) & 0xff, (a >> 24) & 0xff);
    if (id != -1)
        printf(", id: %d }\n", id);
    else
        printf(" }\n");
}

/*
struct connection_tracker *tracker_new(in_addr_t addr) {
    if (trackerp == NUM_TRACKERS)
        exit(1); // TODO
    struct connection_tracker *t = &trackers[trackerp++];
    memset(t, 0, sizeof(*t));
    t->addr = addr;
    t->start_time = SDL_GetTicks64();
    return t;
}
*/

static void *net_thread_main(void *arg) {
    int sockfd = (int)(intptr_t)arg;

    while (!should_quit) {
        int free = -1;
        for (int i = 0; i < MAX_CONNS; i++) {
            if (conns[i].fd == -1) {
                free = i;
                break;
            }
        }
        if (free == -1) {
            printf("no free slot\n");
            exit(1); // TODO
        }
        struct connection *c = &conns[free];

        int connfd;
        struct sockaddr_in connaddr;
        socklen_t connlen = sizeof(connaddr);
        connfd = accept(sockfd, (struct sockaddr *) &connaddr, &connlen);
        if (IS_REAL_ERROR(connfd)) {
            perror("accept");
            exit(1); // TODO
        } else if (connfd != -1) { // we found a new connection (no error and no wouldblock)
            set_nonblocking(connfd);
            c->fd = connfd;
            c->addr = connaddr;
            c->recv_read_pos = c->recv_write_pos = 0;
            c->send_read_pos = c->send_write_pos = 0;
            // c->tracker = tracker_new(c->addr.sin_addr.s_addr);

            printf("accepted ");
            connection_print(c, free);
        }

        // move through all connections
        struct pixel px;
        for (int i = 0; i < MAX_CONNS; i++) {
            c = &conns[i];
            if (c->fd == -1)
                continue;
            // the client is only allowed to send a command to the server if it has
            // enough space in its send buffer for us to be able to incorporate the
            // command.
            int sendbuf_size = c->send_write_pos - c->send_read_pos;
            if (sendbuf_size == CONN_BUF_SIZE) {
                continue;
            }

            int status = connection_recv(c, &px);
            if (status == COMMAND_FAULTY) {
                // do nothing
            } else if (status == COMMAND_PRINT) {
                if (!canvas_set_px(&px)) {
                    // conns[i].tracker->num_pixels_outside_canvas++;
                }
            } else if (status == COMMAND_GET) {
                int inside_canvas = canvas_get_px(&px);
                // put pixel data in sendbuffer. We already know we have enough space.
                // If coordinates are outside range, send (0,0,0)
                // we can't send nothing because the client expects pixel data,
                // and it might be easier to avoid edge conditions
                // TODO perhaps a flag if it was inside or outside range, but then
                // 4 bytes won't be enough anymore
                if (c->send_write_pos == CONN_BUF_SIZE) { // need to move the buffer
                    memmove(c->sendbuf, &c->sendbuf[c->send_read_pos], sendbuf_size);
                    c->send_read_pos = 0;
                    c->send_write_pos = sendbuf_size;
                }
                unsigned char *sp = &c->sendbuf[c->send_write_pos];
                c->send_write_pos += 4;
                sp[0] = px.r;
                sp[1] = px.g;
                sp[2] = px.b;
                sp[3] = inside_canvas;
            } else if (status == COMMAND_WOULDBLOCK) {
                // do nothing
            } else if (status == COMMAND_CONNECTION_END) {
                printf("close ");
                connection_print(c, i);

                connection_close(c);
                continue;
            } else {
                printf("wtf");
                exit(1);
            }

            // try writing the sendbuffer (nonblock)
            // this only happens if the connection is not closed yet
            sendbuf_size = c->send_write_pos - c->send_read_pos; // might have changed
            if (sendbuf_size > 0) {
                status = write(c->fd, &c->sendbuf[c->send_read_pos], sendbuf_size);
                if (IS_REAL_ERROR(status)) {
                    perror("write");
                    exit(1); // TODO
                }
                if (status > 0) {
                    c->send_read_pos += status;
                }
            }
        }
    }

    printf("closing network\n");
    for (int i = 0; i < MAX_CONNS; i++) {
        if (conns[i].fd != -1) {
            connection_close(&conns[i]);
        }
    }
    close(sockfd);
    return NULL;
}

void net_start(void) {
    for (int i = 0; i < MAX_CONNS; i++) {
        conns[i].fd = -1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(0);
    }

    struct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) != 0) {
        perror("bind");
        exit(1);
    }

    if (listen(sockfd, 5) != 0) {
        perror("listen");
        exit(1);
    }

    set_nonblocking(sockfd);

    if (pthread_create(&net_thread, NULL, net_thread_main, (void*)(intptr_t)sockfd) != 0) {
        printf("pthread_create\n");
        exit(1);
    }
}

void net_stop(void) {
    should_quit = 1;
    pthread_join(net_thread, NULL);

    /*
    for (int i = 0; i < trackerp; i++) {
        struct connection_tracker *t = &trackers[i];
        printf("Tracker {\n");
        printf("  ip: %d.%d.%d.%d,\n", t->addr & 0xff, (t->addr >> 8) & 0xff, (t->addr >> 16) & 0xff, (t->addr >> 24) & 0xff);
        printf("  start_time: %lld,\n", t->start_time);
        printf("  end_time: %lld,\n", t->end_time);
        printf("  num_pixels_written: %lld,\n", t->num_pixels_written);
        printf("  num_read_syscalls: %lld,\n", t->num_read_syscalls);
        printf("  num_read_syscalls_blocked: %lld,\n", t->num_read_syscalls_blocked);
        printf("  num_bytes_read: %lld\n", t->num_bytes_read);
        printf("  num_pixels_outside_canvas: %lld\n", t->num_pixels_outside_canvas);
        printf("}\n");
    }
    */
}
