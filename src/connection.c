#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <SDL3/SDL.h>

#include "buffer.h"
#include "param.h"
#include "common.h"
#include "canvas.h"
#include "connection.h"

#define CONN_BUF_SIZE 1024

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
    connection_tracker_init(&c->tracker, connaddr.sin_addr.s_addr, SDL_GetTicks());
    buffer_init_malloc(&c->recvbuf, CONN_BUF_SIZE);
    buffer_init_malloc(&c->sendbuf, CONN_BUF_SIZE);
    c->had_error = false;
}

void connection_close(struct connection *c) {
    buffer_destroy_malloc(&c->recvbuf);
    buffer_destroy_malloc(&c->sendbuf);
    c->tracker.end_time = SDL_GetTicks(); // TODO use OS functionality
    connection_tracker_print(&c->tracker);

    close(c->fd);
    c->fd = -1;
}

static void decode_pixel(struct pixel *px, const unsigned char *rp) {
    px->x = rp[1] | (rp[2] << 8);
    px->y = rp[3] | (rp[4] << 8);
    px->r = rp[5];
    px->g = rp[6];
    px->b = rp[7];
}

#define ENCODE_LE32(value, ptr) do { \
    (ptr)[0] = (value) & 0xff; \
    (ptr)[1] = ((value) >> 8) & 0xff; \
    (ptr)[2] = ((value) >> 16) & 0xff; \
    (ptr)[3] = ((value) >> 24) & 0xff; \
} while (0)

#define PROTOCOL_VERSION 1
#define ENCODE_SIZE 20

static void encode_info(unsigned char *wp) {
    ENCODE_LE32(PROTOCOL_VERSION, wp);
    ENCODE_LE32(TEX_SIZE_X, wp + 4);
    ENCODE_LE32(TEX_SIZE_Y, wp + 8);
    ENCODE_LE32(CONN_BUF_SIZE, wp + 12);
    ENCODE_LE32(CONN_BUF_SIZE, wp + 16);
}

static void get_and_encode_color(struct pixel *px, unsigned char *wp) {
    int inside_canvas = canvas_get_px(px);
    wp[0] = px->r;
    wp[1] = px->g;
    wp[2] = px->b;
    wp[3] = inside_canvas;
}

static enum connection_step_result connection_send(struct connection *c) {
    if (buffer_size(&c->sendbuf) > 0) {
        int status = buffer_write_syscall(&c->sendbuf, c->fd);
        if (IS_REAL_ERROR(status)) {
            c->had_error = true;
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

// TODO perhaps a byte-stream oriented buffer interface? Probably less efficient, though.
enum connection_step_result connection_step(struct connection *c) {
    if (c->had_error) {
        PANIC("called step with faulty connection\n");
    }
    unsigned char *wp = NULL;
    const unsigned char *rp = NULL;
    struct pixel px;
    int status = -1;

    // no reserve happening here, because we're not sure if we can execute the command.
    rp = buffer_read_peek(&c->recvbuf, 8);
    if (rp == NULL) {
        status = buffer_read_syscall(&c->recvbuf, c->fd);
        if (IS_REAL_ERROR(status)) {
            c->had_error = true;
            return CONNECTION_ERR;
        } else if (status == 0) {
            return CONNECTION_END;
        } else if (status > 0) {
            rp = buffer_read_peek(&c->recvbuf, 8);
        }
    }
    if (rp == NULL) {
        goto after_command;
    }

    if (rp[0] == 'I') {
        if ((wp = buffer_write_reserve(&c->sendbuf, ENCODE_SIZE)) != NULL) {
            encode_info(wp);
            buffer_read_reserve(&c->recvbuf, 8);
        }
    } else if (rp[0] == 'P') {
        decode_pixel(&px, rp);
        canvas_set_px(&px);
        buffer_read_reserve(&c->recvbuf, 8);
    } else if (rp[0] == 'G') {
        if ((wp = buffer_write_reserve(&c->sendbuf, 4)) != NULL) {
            px.x = rp[1] | (rp[2] << 8);
            px.y = rp[3] | (rp[4] << 8);
            get_and_encode_color(&px, wp);
            buffer_read_reserve(&c->recvbuf, 8);
        }
    } else {
        // unknown command. Skip.
        buffer_read_reserve(&c->recvbuf, 8);
        switch (connection_send(c)) {
            case CONNECTION_OK: break;
            case CONNECTION_ERR: return CONNECTION_ERR;
            default: PANIC("unreachable");
        }
        return CONNECTION_UNKNOWN_COMMAND;
    }

// always give this connection chance to send some bytes, even if no command was processed.
after_command:
    return connection_send(c);
}
