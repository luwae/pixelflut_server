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

#define NUM_TRACKERS 1024
struct connection_tracker {
    in_addr_t addr;
    unsigned long long start_time;
    unsigned long long end_time;
    unsigned long long num_pixels_read;
    unsigned long long num_read_syscalls;
    unsigned long long num_read_syscalls_blocked;
    unsigned long long num_bytes_read;
    unsigned long long num_pixels_outside_canvas;
};
struct connection_tracker trackers[NUM_TRACKERS];
int trackerp = 0;

#define CONN_BUF_SIZE 1024
struct connection {
    struct connection_tracker *tracker;
    int fd; // fd == -1 means free
    struct sockaddr_in addr;
    int read_pos;
    int write_pos;
    unsigned char buf[CONN_BUF_SIZE];
};

#define GET_SUCCESS 0
#define GET_WOULDBLOCK 1
#define GET_CONNECTION_END 2

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
    conn->tracker->end_time = SDL_GetTicks64(); // TODO use OS functionality
    close(conn->fd);
    conn->fd = -1;
}

// TODO incorporate alpha, or do some other funny commands (mix-multiply, mix-add)
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

static int connection_get_from_buffer(struct connection *conn, struct pixel *px) {
    if (conn->write_pos - conn->read_pos < 8)
        return 0;
    int rp = conn->read_pos;
    conn->read_pos += 8; // advance
    if (conn->buf[rp] != 'P')
        return 0;
    px->x = conn->buf[rp + 1] | (conn->buf[rp + 2] << 8);
    px->y = conn->buf[rp + 3] | (conn->buf[rp + 4] << 8);
    px->r = conn->buf[rp + 5];
    px->g = conn->buf[rp + 6];
    px->b = conn->buf[rp + 7];
    return 1;
}

static int connection_get(struct connection *conn, struct pixel *px) {
    if (connection_get_from_buffer(conn, px)) { // fast path
        conn->tracker->num_pixels_read++;
        return GET_SUCCESS;
    }
    int nc = conn->write_pos - conn->read_pos;
    if (conn->read_pos > 0) {
        // there is not enough in the buffer. copy the rest and try reading
        // we can use memcpy because there are less than 8 bytes to be copied
        memcpy(conn->buf, &conn->buf[conn->read_pos], nc);
        conn->read_pos = 0;
        conn->write_pos = nc;
    }
    int status = read(conn->fd, &conn->buf[conn->write_pos], CONN_BUF_SIZE - conn->write_pos);
    conn->tracker->num_read_syscalls++;
    if (WOULD_BLOCK(status)) {
        conn->tracker->num_read_syscalls_blocked++;
        return GET_WOULDBLOCK;
    }
    else if (status == -1) {
        perror("read");
        exit(1); // TODO
    } else if (status == 0) {
        return GET_CONNECTION_END;
    } else {
        conn->write_pos += status;
        conn->tracker->num_bytes_read += status;
        if (connection_get_from_buffer(conn, px)) {
            conn->tracker->num_pixels_read++;
            return GET_SUCCESS;
        } else {
            conn->tracker->num_read_syscalls_blocked++;
            return GET_WOULDBLOCK;
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

struct connection_tracker *tracker_new(in_addr_t addr) {
    if (trackerp == NUM_TRACKERS)
        exit(1); // TODO
    struct connection_tracker *t = &trackers[trackerp++];
    memset(t, 0, sizeof(*t));
    t->addr = addr;
    t->start_time = SDL_GetTicks64();
    return t;
}

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
        } else if (connfd != -1) {
            set_nonblocking(connfd);
            c->fd = connfd;
            c->addr = connaddr;
            c->read_pos = c->write_pos = 0;
            c->tracker = tracker_new(c->addr.sin_addr.s_addr);

            printf("accepted ");
            connection_print(c, free);
        }

        // move through all connections
        struct pixel px;
        for (int i = 0; i < MAX_CONNS; i++) {
            if (conns[i].fd == -1)
                continue;
            int status = connection_get(&conns[i], &px);
            if (status == GET_SUCCESS) {
                // printf("Pixel { x: %u y: %u col: (%d, %d, %d) } from ", px.x, px.y, px.r, px.g, px.b);
                // connection_print(&conns[i], i);
                if (!canvas_set_px(&px))
                    conns[i].tracker->num_pixels_outside_canvas++;
            } else if (status == GET_WOULDBLOCK) {
                // do nothing
            } else { // if status == GET_CONNECTION_END
                printf("close ");
                connection_print(&conns[i], i);

                connection_close(&conns[i]);
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

    for (int i = 0; i < trackerp; i++) {
        struct connection_tracker *t = &trackers[i];
        printf("Tracker {\n");
        printf("  ip: %d.%d.%d.%d,\n", t->addr & 0xff, (t->addr >> 8) & 0xff, (t->addr >> 16) & 0xff, (t->addr >> 24) & 0xff);
        printf("  start_time: %lld,\n", t->start_time);
        printf("  end_time: %lld,\n", t->end_time);
        printf("  num_pixels_read: %lld,\n", t->num_pixels_read);
        printf("  num_read_syscalls: %lld,\n", t->num_read_syscalls);
        printf("  num_read_syscalls_blocked: %lld,\n", t->num_read_syscalls_blocked);
        printf("  num_bytes_read: %lld\n", t->num_bytes_read);
        printf("  num_pixels_outside_canvas: %lld\n", t->num_pixels_outside_canvas);
        printf("}\n");
    }
}
