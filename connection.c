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

void rect_iter_init(struct rect_iter *r) {
    memset(r, 0, sizeof(*r));
}

int rect_iter_done(const struct rect_iter *r) {
    return r->y == r->ystop || r->xstart == r->xstop;
}

void rect_iter_advance(struct rect_iter *r) {
    if (rect_iter_done(r)) {
        printf("WARNING: rect_iter_advance called on finished iter\n");
        return; // TODO panic?
    }
    r->x += 1;
    if (r->x == r->xstop) {
        r->x = r->xstart;
        r->y += 1;
    }
}

void connection_print(const struct connection *c) {
    in_addr_t a = c->addr.sin_addr.s_addr;
    printf("Connection { ip: %d.%d.%d.%d }\n", a & 0xff, (a >> 8) & 0xff, (a >> 16) & 0xff, (a >> 24) & 0xff);
}

void connection_init(struct connection *c, int connfd, struct sockaddr_in connaddr) {
    set_nonblocking(connfd);
    c->fd = connfd;
    c->addr = connaddr;
    connection_tracker_init(&c->tracker, connaddr.sin_addr.s_addr, SDL_GetTicks64());
    rect_iter_init(&c->multirecv);
    rect_iter_init(&c->multisend);
    buffer_init_malloc(&c->recvbuf);
    buffer_init_malloc(&c->sendbuf);
}

void connection_close(struct connection *c) {
    buffer_destroy_malloc(&c->recvbuf);
    buffer_destroy_malloc(&c->sendbuf);
    c->tracker.end_time = SDL_GetTicks64(); // TODO use OS functionality
    connection_tracker_print(&c->tracker);

    close(c->fd);
    c->fd = -1;
}

int connection_recv_from_multi(struct connection *c, struct pixel *px) {
    if (!rect_iter_done(&c->multirecv)) {
        const unsigned char *p = buffer_read_reserve(&c->recvbuf, 4);
        if (p == NULL) {
            return COMMAND_NONE;
        }
        px->x = c->multirecv.x;
        px->y = c->multirecv.y;
        px->r = p[0];
        px->g = p[1];
        px->b = p[2];
        rect_iter_advance(&c->multirecv);
        return COMMAND_MULTIRECV_NEXT;
    } else {
        return COMMAND_MULTIRECV_DONE;
    }
}

// TODO this interface is stupid. Pull some of the work from net.c here
int connection_recv_from_buffer(struct connection *c, struct pixel *px) {
    int status = connection_recv_from_multi(c, px);
    if (status != COMMAND_MULTIRECV_DONE)
        return status;

    // we need to read the next 'real' command
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
    } else if (p[0] == 'p') {
        struct rect_iter *r = &c->multirecv;
        if (!rect_iter_done(r)) {
            printf("ERROR: multirecv not empty although it should be\n");
            exit(1); // TODO
        }
        r->xstart = p[1] | (p[2] << 8);
        r->x = r->xstart;
        r->ystart = p[3] | (p[4] << 8);
        r->y = r->ystart;
        int w = p[5] | ((p[7] & 0x0f) << 8);
        int h = p[6] | ((p[7] & 0xf0) << 4);
        r->xstop = r->xstart + w;
        r->ystop = r->ystart + h;
        return COMMAND_MULTIRECV;
    } else if (p[0] == 'g') {
        struct rect_iter *r = &c->multisend;
        if (!rect_iter_done(r)) {
            printf("ERROR: multisend not empty although it should be\n");
            exit(1); // TODO
        }
        r->xstart = p[1] | (p[2] << 8);
        r->x = r->xstart;
        r->ystart = p[3] | (p[4] << 8);
        r->y = r->ystart;
        int w = p[5] | ((p[7] & 0x0f) << 8);
        int h = p[6] | ((p[7] & 0xf0) << 4);
        r->xstop = r->xstart + w;
        r->ystop = r->ystart + h;
        return COMMAND_MULTISEND;
    } else if (p[0] == 'I') {
        return COMMAND_INFO;
    } else {
        return COMMAND_FAULTY;
    }
}

int connection_recv(struct connection *c, struct pixel *px) {
    int status = connection_recv_from_buffer(c, px); // fast path - we already read enough
    if (status != COMMAND_NONE) {
        return status;
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
