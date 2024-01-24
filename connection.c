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

void connection_tracker_print(const struct connection_tracker *t) {
    printf("Tracker {\n");
    printf("  ip: %d.%d.%d.%d,\n", t->addr & 0xff, (t->addr >> 8) & 0xff, (t->addr >> 16) & 0xff, (t->addr >> 24) & 0xff);
    printf("  start_time: %lld,\n", t->start_time);
    printf("  end_time: %lld,\n", t->end_time);
    printf("}\n");
}

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

void connection_init(struct connection *c, int connfd, struct sockaddr_in connaddr, int id) {
    set_nonblocking(connfd);
    memset(c, 0, sizeof(*c)); // TODO not the buffers?
    c->fd = connfd;
    c->addr = connaddr;
    c->tracker.start_time = SDL_GetTicks64(); // TODO use OS functionalityy
    c->tracker.addr = connaddr.sin_addr.s_addr;

    printf("accepted ");
    connection_print(c, id);
}

void connection_close(struct connection *c) {
    c->tracker.end_time = SDL_GetTicks64(); // TODO use OS functionality
    connection_tracker_print(&c->tracker);

    close(c->fd);
    c->fd = -1;
}

int rect_iter_done(const struct rect_iter *r) {
    return r->y == r->ystop || r->xstart == r->xstop;
}

void rect_iter_advance(struct rect_iter *r) {
    if (rect_iter_done(r))
        return; // TODO panic?
    r->x += 1;
    if (r->x == r->xstop) {
        r->x = r->xstart;
        r->y += 1;
    }
}

unsigned long long buffer_size(const struct buffer *b) {
    return b->write_pos - b->read_pos;
}

const unsigned char *buffer_read_reserve(struct buffer *b, unsigned long long n) {
    const unsigned char *p = NULL;
    if (buffer_size(b) >= n) {
        p = &b->data[b->read_pos];
        b->read_pos += n;
    }
    return p;
}

unsigned char *buffer_write_reserve(struct buffer *b, unsigned long long n) {
    unsigned char *p = NULL;
    if (CONN_BUF_SIZE - b->write_pos >= n) {
        p = &b->data[b->write_pos];
        b->write_pos += n;
    }
    return p;
}

void buffer_move_front(struct buffer *b) {
    int bufsize = b->write_pos - b->read_pos;
    if (b->read_pos > 0) {
        memmove(b->data, &b->data[b->read_pos], bufsize);
        b->read_pos = 0;
        b->write_pos = bufsize;
    }
}

int buffer_recv_nonblocking(struct buffer *b, int fd) {
    buffer_move_front(b);
    if (b->write_pos == CONN_BUF_SIZE) {
        printf("don't call recv on full buffer\n");
        return -1; // TODO
    }
    int status = read(fd, &b->data[b->write_pos], CONN_BUF_SIZE - b->write_pos);
    if (status > 0) {
        b->write_pos += status;
    }
    return status;
}

int buffer_send_nonblocking(struct buffer *b, int fd) {
    int bufsize = buffer_size(b);
    if (bufsize == 0) {
        printf("don_t call send on empty buffer\n");
        return -1; // TODO
    }
    int status = write(fd, &b->data[b->read_pos], bufsize);
    if (status > 0) {
        b->read_pos += status;
    }
    return status;
}

int connection_recv_from_multi(struct connection *conn, struct pixel *px) {
    const unsigned char *rb:
    if (!rect_iter_done(&conn->multirecv)) {
        const unsigned char *rp = buffer_read_reserve(&conn->recvbuf, 4);
        if (rb == NULL)
            return COMMAND_NONE;
        px->x = conn->multirecv.x;
        px->y = conn->multirecv.y;
        px->r = rp[0];
        px->g = rp[1];
        px->b = rp[2];
        rect_iter_advance(&conn->multirecv);
        return COMMAND_MULTIRECV;
    } else {
        return COMMAND_MULTIRECV_DONE;
    }
}
/*
 * 'p' / 'g'
 * x (lo)
 * x (hi)
 * y (lo)
 * y (hi)
 * w [0..=7]
 * h [0..=7]
 * high to low: h[11] h[10] h[9] h[8] w[11] w[10] w[9] w[8]
 */

int connection_recv_from_buffer(struct connection *conn, struct pixel *px) {
    int status = connection_recv_from_multi(conn, px);
    if (status != COMMAND_MULTIRECV_DONE)
        return status;

    // we need to read the next 'real' command
    const unsigned char *rp = buffer_read_reserve(&conn->recvbuf, 8);
    if (rp == NULL)
        return COMMAND_NONE;
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
    } else if (rp[0] == 'p') {
        struct rect_iter *r = &conn->multirecv;
        if (!rect_iter_done(r)) {
            printf("ERROR: multirecv not empty\n");
            exit(1) // TODO
        }
        r->xstart = rp[1] | (rp[2] << 8);
        r->x = r->xstart;
        r->ystart = rp[3] | (rp[4] << 8);
        r->y = r->ystart;
        int w = rp[5] | ((rp[7] & 0x0f) << 8);
        int h = rp[6] | ((rp[7] & 0xf0) << 4);
        if (w == 0 || h == 0) {
            r->xstop = r->xstart;
            r->ystop = r->ystart;
            return COMMAND_FAULTY; // it's not a fault based on the spec, but this is fine here
                                   // (since we just ignore it)
        }
        r->xstop = r->xstart + w;
        r->ystop = r->ystart + h;
        return connection_recv_from_multi(conn, px); // TODO assert that this is not COMMAND_MULTIRECV_DONE
                                                  // TODO do we want this here immediately? or really only read <=8 bytes per call?
    } else if (rp[0] == 'g') {
        struct rect_iter *r = &conn->multisend;
        if (!rect_iter_done(r)) {
            printf("ERROR: multisend not empty\n");
            exit(1) // TODO
        }
        r->xstart = rp[1] | (rp[2] << 8);
        r->x = r->xstart;
        r->ystart = rp[3] | (rp[4] << 8);
        r->y = r->ystart;
        int w = rp[5] | ((rp[7] & 0x0f) << 8);
        int h = rp[6] | ((rp[7] & 0xf0) << 4);
        if (w == 0 || h == 0) {
            r->xstop = r->xstart;
            r->ystop = r->ystart;
            return COMMAND_FAULTY; // it's not a fault based on the spec, but this is fine here
                                   // (since we just ignore it)
        }
        r->xstop = r->xstart + w;
        r->ystop = r->ystart + h;
        return COMMAND_MULTISEND;
    } else {
        return COMMAND_FAULTY;
    }
}

int connection_recv(struct connection *conn, struct pixel *px) {
    int status = connection_recv_from_buffer(conn, px); // fast path - we already read enough
    if (status != COMMAND_NONE)
        return status; // on faulty command, client is skipped (does nothing).

    status = buffer_recv_nonblocking(&conn->recvbuf, conn->fd);
    if (WOULD_BLOCK(status)) {
        return COMMAND_WOULDBLOCK;
    } else if (status == -1) {
        perror("read");
        exit(1); // TODO
    } else if (status == 0) {
        return COMMAND_CONNECTION_END;
    } else {
        status = connection_recv_from_buffer(conn, px);
        if (status != COMMAND_NONE) {
            return status;
        } else {
            // we still don't have enough bytes. just interpret this as wouldblock.
            // don't increase num_read_syscalls_wouldblock here
            return COMMAND_WOULDBLOCK;
        }
    }
}

void connection_print(const struct connection *conn, int id) {
    in_addr_t a = conn->addr.sin_addr.s_addr;
    printf("Connection { ip: %d.%d.%d.%d", a & 0xff, (a >> 8) & 0xff, (a >> 16) & 0xff, (a >> 24) & 0xff);
    if (id != -1)
        printf(", id: %d }\n", id);
    else
        printf(" }\n");
}

