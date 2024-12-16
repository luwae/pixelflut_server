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
#include "canvas.h"
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
    c->fd = -1; // extra security
}

static void decode_rect(struct rect_iter *r, const unsigned char *rp) {
    r->xstart = rp[1] | (rp[2] << 8);
    r->x = r->xstart;
    r->ystart = rp[3] | (rp[4] << 8);
    r->y = r->ystart;
    int w = rp[5] | ((rp[7] & 0x0f) << 8);
    int h = rp[6] | ((rp[7] & 0xf0) << 4);
    r->xstop = r->xstart + w;
    r->ystop = r->ystart + h;
}

static void decode_pixel(struct pixel *px, const unsigned char *rp) {
    px->x = rp[1] | (rp[2] << 8);
    px->y = rp[3] | (rp[4] << 8);
    px->r = rp[5];
    px->g = rp[6];
    px->b = rp[7];
}

static void decode_color(struct pixel *px, const unsigned char *rp) {
    px->r = rp[0];
    px->g = rp[1];
    px->b = rp[2];
}

#define ENCODE_LE32(value, ptr) do { \
    (ptr)[0] = (value) & 0xff; \
    (ptr)[1] = ((value) >> 8) & 0xff; \
    (ptr)[2] = ((value) >> 16) & 0xff; \
    (ptr)[3] = ((value) >> 24) & 0xff; \
} while (0)

static void encode_info(unsigned char *wp) {
    ENCODE_LE32(TEX_SIZE_X, wp);
    ENCODE_LE32(TEX_SIZE_Y, wp + 4);
    ENCODE_LE32(CONN_BUF_SIZE, wp + 8);
    ENCODE_LE32(CONN_BUF_SIZE, wp + 12);
}

static void get_and_encode_color(struct pixel *px, unsigned char *wp) {
    int inside_canvas = canvas_get_px(px);
    wp[0] = px->r;
    wp[1] = px->g;
    wp[2] = px->b;
    wp[3] = inside_canvas;
}

static int connection_send(struct connection *c) {
    if (buffer_size(&c->sendbuf) > 0) {
        int status = buffer_write_syscall(&c->sendbuf, c->fd);
        if (IS_REAL_ERROR(status)) {
            return CONNECTION_ERR;
        }
    }
    return CONNECTION_OK;
}

/* In each iteration, the client is allowed
 * - up to 1 read() syscall. To maximize efficiency, it always happens as late as possible (and only if needed).
 * - up to 1 write() syscall. This happens at the end. The send buffer should be filled as much as possible.
 * - up to 1 drawn pixel.
 */

#define DRAW_LIMIT 1
#define READ_LIMIT 1

// TODO perhaps a byte-stream oriented buffer interface? Probably less efficient, though.
int connection_step(struct connection *c) {
    unsigned char *wp;
    const unsigned char *rp;
    struct pixel px;
    int have_read = 0;
    int have_drawn = 0;
    int multisend_done; // this is extra protection against another command trying to pack its response in the middle of a multisend sequence
                        // doesn't happen as long as we don't have responses < 4 bytes.
    int status;
    // while loop here because we might go through several multirecvs/multisends in one connection_step
    while (1) {
        // 1. handle multi send as far as possible
        while (!rect_iter_done(&c->multisend) && (wp = buffer_write_reserve(&c->sendbuf, 4)) != NULL) {
            px.x = c->multisend.x;
            px.y = c->multisend.y;
            rect_iter_advance(&c->multisend);
            get_and_encode_color(&px, wp);
        }

        // 2. handle multi recv as far as possible
        while (!rect_iter_done(&c->multirecv) && have_drawn < DRAW_LIMIT) {
            if (c->multirecv_source == MULTIRECV_SOURCE_FILL) {
                goto do_multirecv; // no reading necessary
            }
            rp = buffer_read_reserve(&c->recvbuf, 4);
            if (rp == NULL && have_read < READ_LIMIT) {
                have_read += 1;
                status = buffer_read_syscall(&c->recvbuf, c->fd);
                if (IS_REAL_ERROR(status)) {
                    return CONNECTION_ERR;
                } else if (status == 0) {
                    return CONNECTION_END;
                } else if (status > 0) {
                    rp = buffer_read_reserve(&c->recvbuf, 4);
                }
            }
            if (rp == NULL) {
                return connection_send(c);
            }
            if (c->multirecv_source == MULTIRECV_SOURCE_FILL_NOT_READ) {
                c->multirecv_source = MULTIRECV_SOURCE_FILL;
                c->multirecv_source_fill_r = rp[0];
                c->multirecv_source_fill_g = rp[1];
                c->multirecv_source_fill_b = rp[2];
            }
do_multirecv:
            px.x = c->multirecv.x;
            px.y = c->multirecv.y;
            if (c->multirecv_source == MULTIRECV_SOURCE_FILL) {
                px.r = c->multirecv_source_fill_r;
                px.g = c->multirecv_source_fill_g;
                px.b = c->multirecv_source_fill_b;
            } else { // TODO ASSERT MULTIRECV_SOURCE_INDIVIDUAL
                decode_color(&px, rp);
            }
            rect_iter_advance(&c->multirecv);
            canvas_set_px(&px);
            have_drawn += 1;
        }
        if (!rect_iter_done(&c->multirecv)) {
            return connection_send(c);
        }

        // 3. get actual command
        // Invariants holding here:
        //  - multisend is either empty or the sendbuffer is full
        //  - multirecv is empty -> we can read an actual command
        // peek here instead of reserve, because we can't be sure that we are able to process the command
        rp = buffer_read_peek(&c->recvbuf, 8);
        if (rp == NULL && have_read < READ_LIMIT) {
            have_read += 1;
            status = buffer_read_syscall(&c->recvbuf, c->fd);
            if (IS_REAL_ERROR(status)) {
                return CONNECTION_ERR;
            } else if (status == 0) {
                return CONNECTION_END;
            } else if (status > 0) {
                rp = buffer_read_peek(&c->recvbuf, 8);
            }
        }
        if (rp == NULL) {
            return connection_send(c);
        }

        multisend_done = rect_iter_done(&c->multisend);
        if (rp[0] == 'I') {
            if (!multisend_done || (wp = buffer_write_reserve(&c->sendbuf, 16)) == NULL) {
                return connection_send(c);
            }
            encode_info(wp);
        } else if (rp[0] == 'P') {
            if (have_drawn == DRAW_LIMIT) {
                return connection_send(c);
            }
            decode_pixel(&px, rp);
            canvas_set_px(&px);
            have_drawn += 1;
        } else if (rp[0] == 'G') {
            if (!multisend_done || (wp = buffer_write_reserve(&c->sendbuf, 4)) == NULL) {
                return connection_send(c);
            }
            px.x = rp[1] | (rp[2] << 8);
            px.y = rp[3] | (rp[4] << 8);
            get_and_encode_color(&px, wp);
        } else if (rp[0] == 'p') {
            if (!rect_iter_done(&c->multirecv)) {
                printf("BUG: multirecv not empty?\n");
                return CONNECTION_ERR;
            }
            c->multirecv_source = MULTIRECV_SOURCE_INDIVIDUAL;
            decode_rect(&c->multirecv, rp);
        } else if (rp[0] == 'f') {
            if (!rect_iter_done(&c->multirecv)) {
                printf("BUG: multirecv not empty?\n");
                return CONNECTION_ERR;
            }
            c->multirecv_source = MULTIRECV_SOURCE_FILL_NOT_READ;
            decode_rect(&c->multirecv, rp);
        } else if (rp[0] == 'g') {
            if (!rect_iter_done(&c->multisend)) {
                return connection_send(c);
            }
            decode_rect(&c->multisend, rp);
        } else {
            // unknown command.
            return CONNECTION_ERR;
        }
        // ADVANCE
        if (buffer_read_reserve(&c->recvbuf, 8) == NULL) {
            printf("BUG: reserve after peek\n");
            return CONNECTION_ERR;
        }
    }
}
