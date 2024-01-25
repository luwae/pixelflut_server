#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include "SDL.h" // only used for timing here

#include "common.h"
#include "connection.h"

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

void connection_tracker_init(struct connection_tracker *t, in_addr_t addr, unsigned long long start_time) {
    memset(t, 0, sizeof(*t));
    t->addr = addr;
    t->start_time = start_time;
}

void connection_tracker_print(const struct connection_tracker *t) {
    printf("Tracker {\n");
    printf("  ip: %d.%d.%d.%d,\n", t->addr & 0xff, (t->addr >> 8) & 0xff, (t->addr >> 16) & 0xff, (t->addr >> 24) & 0xff);
    printf("  start_time: %lld,\n", t->start_time);
    printf("  end_time: %lld,\n", t->end_time);
    printf("}\n");

}

void connection_print(const struct connection *c) {
    in_addr_t a = c->addr.sin_addr.s_addr;
    printf("Connection { ip: %d.%d.%d.%d }\n", a & 0xff, (a >> 8) & 0xff, (a >> 16) & 0xff, (a >> 24) & 0xff);
}

void connection_init(struct connection *c, int connfd, struct sockaddr_in connaddr) {
    set_nonblocking(connfd);
    c->fd = connfd;
    c->addr = connaddr;
    connection_tracker_init(&c->tracker, SDL_GetTicks64(), connaddr.sin_addr.s_addr);
    buffer_init_malloc(&c->recvbuf);
    buffer_init_malloc(&c->sendbuf);

    printf("accepted ");
    connection_print(c);
}

void connection_close(struct connection *c) {
    buffer_destroy_malloc(&c->recvbuf);
    buffer_destroy_malloc(&c->sendbuf);
    c->tracker.end_time = SDL_GetTicks64(); // TODO use OS functionality
    connection_tracker_print(&c->tracker);

    close(c->fd);
    c->fd = -1;
}

int connection_recv_from_buffer(struct connection *c, struct pixel *px) {
    const unsigned char *p = buffer_read_reserve(&c->recvbuf, 8);
    if (p == NULL) {
        return COMMAND_NONE;
    }
    if (p[0] == 'P') {
        px->x = p[1] | (p[2] << 8);
        px->y = p[3] | (p[4] << 8);
        px->r = p[5];
        px->g = p[6];
        px->b = p[7];
        return COMMAND_PRINT;
    } else if (p[0] == 'G') {
        px->x = p[1] | (p[2] << 8);
        px->y = p[3] | (p[4] << 8);
        return COMMAND_GET;
    } else {
        return COMMAND_FAULTY;
    }
}

int connection_recv(struct connection *c, struct pixel *px) {
    int status = connection_recv_from_buffer(c, px); // fast path - we already read enough
    if (status != COMMAND_NONE) {
        return status; // on faulty command, client is skipped (does nothing).
    }

    // we don't have enough bytes. try to get more from socket
    status = buffer_read_syscall(&c->recvbuf, c->fd);
    if (WOULD_BLOCK(status)) {
        return COMMAND_WOULDBLOCK;
    } else if (status == -1) {
        return COMMAND_SYS_ERROR;
    } else if (status == 0) {
        return COMMAND_CONNECTION_END;
    } else {
        // try again
        status = connection_recv_from_buffer(c, px);
        if (status != COMMAND_NONE) {
            return status;
        } else {
            // we still don't have enough bytes. just interpret this as wouldblock
            // NOTE for later: don't increase num_read_syscalls_wouldblock here
            return COMMAND_WOULDBLOCK;
        }
    }
}
