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

#include "param.h"
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

static int connection_send(struct connection *c) {
    if (buffer_size(&c->sendbuf) > 0) {
        status = buffer_write_syscall(&c->sendbuf, c->fd);
        if (IS_REAL_ERROR(status)) {
            return CONNECTION_ERR;
        }
    }
    return CONNECTION_OK;
}

// the read() may happen any time.
// the write() happens right at the end (see return pattern)
int connection_step(struct connection *c) {
    unsigned char *wp;
    const unsigned char *rp;
    struct pixel px;
    int have_read = 0;
    int status;
    while (1) {
        // 1. handle multi send as far as possible
        while (!rect_iter_done(&c->multisend) && (wp = buffer_write_reserve(&c->sendbuf, 4)) != NULL) {
            px.x = c->multisend.x;
            px.y = c->multisend.y;
            rect_iter_advance(&c->multisend);
            int inside_canvas = canvas_get_px(&px);
            wp[0] = px.r;
            wp[1] = px.g;
            wp[2] = px.b;
            wp[3] = inside_canvas;
        }

        // 2. handle multi recv
        if (!rect_iter_done(&c->multirecv)) {
            rp = buffer_read_reserve(&c->recvbuf, 4);
            if (rp == NULL && !have_read) {
                have_read = 1;
                status = buffer_read_syscall(&c->recvbuf);
                if (IS_REAL_ERROR(status)) {
                    return CONNECTION_ERR;
                } else if (status == 0) {
                    return CONNECTION_END;
                } else if (status > 0)
                    rp = buffer_read_reserve(&c->recvbuf, 4);
                }
            }
            if (rp != NULL) {
                px.x = c->multirecv.x;
                px.y = c->multirecv.y;
                px->r = rp[0];
                px->g = rp[1];
                px->b = rp[2];
                rect_iter_advance(&c->multirecv);
                canvas_set_px(&px);
            }
            return connection_send(c); // we either drew a pixel or can't read it. return in either case.
        }

        // 3. get actual command
        // peek here instead of reserve, because we can't be sure that we are able to process the command
        rp = buffer_read_peek(&c->recvbuf, 8);
        if (rp == NULL && !have_read) {
            have_read = 1;
            status = buffer_read_syscall(&c->recvbuf);
            if (IS_REAL_ERROR(status)) {
                return CONNECTION_ERR;
            } else if (status == 0) {
                return CONNECTION_END;
            } else if (status > 0)
                rp = buffer_read_peek(&c->recvbuf, 8);
            }
        }
        // we have tried our very best to read a new command.
        if (rp == NULL) {
            return connection_send(c);
        }

        if (rp[0] == 'I') {
            wp = buffer_write_reserve(&c->sendbuf, 16);
            if (wp == NULL) {
                // command is read again next time.
                return connection_send(c);
            }
            wp[0] = TEX_SIZE_X & 0xff;
            wp[1] = (TEX_SIZE_X >> 8) & 0xff;
            wp[2] = (TEX_SIZE_X >> 16) & 0xff;
            wp[3] = (TEX_SIZE_X >> 24) & 0xff;
            wp[4] = TEX_SIZE_Y & 0xff;
            wp[5] = (TEX_SIZE_Y >> 8) & 0xff;
            wp[6] = (TEX_SIZE_Y >> 16) & 0xff;
            wp[7] = (TEX_SIZE_Y >> 24) & 0xff;
            wp[8] = CONN_BUF_SIZE & 0xff;
            wp[9] = (CONN_BUF_SIZE >> 8) & 0xff;
            wp[10] = (CONN_BUF_SIZE >> 16) & 0xff;
            wp[11] = (CONN_BUF_SIZE >> 24) & 0xff;
            wp[12] = CONN_BUF_SIZE & 0xff;
            wp[13] = (CONN_BUF_SIZE >> 8) & 0xff;
            wp[14] = (CONN_BUF_SIZE >> 16) & 0xff;
            wp[15] = (CONN_BUF_SIZE >> 24) & 0xff;
            // ADVANCE
            buffer_read_reserve(&c->recvbuf, 8); // TODO assert?
        } else if (rp[0] == 'P') {
            px.x = rp[1] | (rp[2] << 8);
            px.y = rp[3] | (rp[4] << 8);
            px.r = rp[5];
            px.g = rp[6];
            px.b = rp[7];
            // ADVANCE
            buffer_read_reserve(&c->recvbuf, 8); // TODO assert?
            canvas_set_px(&px);
            return connection_send(c); // pixel has been placed. return.
        } else if (rp[0] == 'G') {
            wp = buffer_write_reserve(&c->sendbuf, 4);
            if (wp == NULL) {
                // command is read again next time.
                return connection_send(c);
            }
            px.x = rp[1] | (rp[2] << 8);
            px.y = rp[3] | (rp[4] << 8);
            // ADVANCE
            buffer_read_reserve(&c->recvbuf, 8); // TODO assert?
        } else if (rp[0] == 'p') {
            struct rect_iter *r = &c->multirecv;
            if (!rect_iter_done(r)) {
                // command is read again next time.
                return connection_send(c);
            }
            r->xstart = rp[1] | (rp[2] << 8);
            r->x = r->xstart;
            r->ystart = rp[3] | (rp[4] << 8);
            r->y = r->ystart;
            int w = rp[5] | ((rp[7] & 0x0f) << 8);
            int h = rp[6] | ((rp[7] & 0xf0) << 4);
            r->xstop = r->xstart + w;
            r->ystop = r->ystart + h;
            // ADVANCE
            buffer_read_reserve(&c->recvbuf, 8); // TODO assert?
        } else if (rp[0] == 'g') {
            struct rect_iter *r = &c->multisend;
            if (!rect_iter_done(r)) {
                // command is read again next time.
                return connection_send(c);
            }
            r->xstart = rp[1] | (rp[2] << 8);
            r->x = r->xstart;
            r->ystart = rp[3] | (rp[4] << 8);
            r->y = r->ystart;
            int w = rp[5] | ((rp[7] & 0x0f) << 8);
            int h = rp[6] | ((rp[7] & 0xf0) << 4);
            r->xstop = r->xstart + w;
            r->ystop = r->ystart + h;
            // ADVANCE
            buffer_read_reserve(&c->recvbuf, 8); // TODO assert?
        }
    }
}
