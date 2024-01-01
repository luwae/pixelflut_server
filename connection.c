#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>

#include "common.h"
#include "connection.h"

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

int connection_get(struct connection *conn, struct pixel *px) {
    if (connection_get_from_buffer(conn, px)) // fast path
        return GET_SUCCESS;
    int nc = conn->write_pos - conn->read_pos;
    if (conn->read_pos > 0) {
        // there is not enough in the buffer. copy the rest and try reading
        // we can use memcpy because there are less than 8 bytes to be copied
        memcpy(conn->buf, &conn->buf[conn->read_pos], nc);
        conn->read_pos = 0;
        conn->write_pos = nc;
    }
    int status = read(conn->fd, &conn->buf[conn->write_pos], CONN_BUF_SIZE - conn->write_pos);
    if (WOULD_BLOCK(status))
        return GET_WOULDBLOCK;
    else if (status == -1) {
        perror("read");
        exit(1);
    } else if (status == 0) {
        return GET_CONNECTION_END;
    } else {
        conn->write_pos += status;
        return connection_get_from_buffer(conn, px) ? GET_SUCCESS : GET_WOULDBLOCK;
    }
}

void connection_close(struct connection *conn) {
    close(conn->fd);
    conn->fd = -1;
}

