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
    printf("  num_bytes_received_from_client: %lld,\n", t->num_bytes_received_from_client);
    printf("  num_command_print: %lld,\n", t->num_command_print);
    printf("  num_command_get: %lld,\n", t->num_command_get);
    printf("  num_read_syscalls: %lld,\n", t->num_read_syscalls);
    printf("  num_read_syscalls_wouldblock: %lld,\n", t->num_read_syscalls_wouldblock);
    printf("  num_bytes_sent_to_client: %lld,\n", t->num_bytes_sent_to_client);
    printf("  num_pixels_sent_to_client: %lld,\n", t->num_pixels_sent_to_client);
    printf("  num_write_syscalls: %lld,\n", t->num_write_syscalls);
    printf("  num_write_syscalls_wouldblock: %lld,\n", t->num_write_syscalls_wouldblock);
    printf("  num_coords_outside_canvas: %lld,\n", t->num_coords_outside_canvas);
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


int connection_recv_from_buffer(struct connection *conn, struct pixel *px) {
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

int connection_recv(struct connection *conn, struct pixel *px) {
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
    conn->tracker.num_read_syscalls += 1;
    if (WOULD_BLOCK(status)) {
        conn->tracker.num_read_syscalls_wouldblock += 1;
        return COMMAND_WOULDBLOCK;
    } else if (status == -1) {
        perror("read");
        exit(1); // TODO
    } else if (status == 0) {
        return COMMAND_CONNECTION_END;
    } else {
        conn->recv_write_pos += status;
        conn->tracker.num_bytes_received_from_client += status;
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

void connection_print(const struct connection *conn, int id) {
    in_addr_t a = conn->addr.sin_addr.s_addr;
    printf("Connection { ip: %d.%d.%d.%d", a & 0xff, (a >> 8) & 0xff, (a >> 16) & 0xff, (a >> 24) & 0xff);
    if (id != -1)
        printf(", id: %d }\n", id);
    else
        printf(" }\n");
}

