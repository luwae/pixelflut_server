#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "param.h"
#include "buffer.h"

void buffer_init_malloc(struct buffer *b) {
    b->read_pos = b->write_pos = 0;
    b->data = malloc(CONN_BUF_SIZE);
    if (b->data == NULL) {
        perror("malloc");
        exit(1); // TODO
    }
}

void buffer_destroy_malloc(struct buffer *b) {
    free(b->data);
    b->data = NULL;
}

size_t buffer_size(const struct buffer *b) {
    return b->write_pos - b->read_pos;
}

size_t buffer_write_space(const struct buffer *b) {
    return CONN_BUF_SIZE - b->write_pos;
}

void buffer_move_front(struct buffer *b) {
    if (b->read_pos > 0) {
        size_t size = buffer_size(b);
        memmove(b->data, &b->data[b->read_pos], size);
        b->read_pos = 0;
        b->write_pos = size;
    }
}

int buffer_read_syscall(struct buffer *b, int fd) {
    size_t size = buffer_size(b);
    if (size == CONN_BUF_SIZE) {
        printf("don't call read when buffer full");
        exit(1); // TODO
    }
    buffer_move_front(b);
    int status = read(fd, &b->data[b->write_pos], buffer_write_space(b));
    if (status > 0) {
        b->write_pos += status;
    }
    return status;
}

int buffer_write_syscall(struct buffer *b, int fd) {
    size_t size = buffer_size(b);
    if (size == 0) {
        printf("don't call write when buffer empty");
        exit(1); // TODO
    }
    int status = write(fd, &b->data[b->read_pos], size);
    if (status > 0) {
        b->read_pos += status;
    }
    buffer_move_front(b); // after because then we have less bytes to move
    return status;
}

const unsigned char *buffer_read_reserve(struct buffer *b, size_t size) {
    const unsigned char *p = NULL;
    if (buffer_size(b) >= size) {
        p = &b->data[b->read_pos];
        b->read_pos += size;
    }
    return p;
}

unsigned char *buffer_write_reserve(struct buffer *b, size_t size) {
    unsigned char *p = NULL;
    if (buffer_write_space(b) >= size) {
        p = &b->data[b->write_pos];
        b->write_pos += size;
    }
    return p;
}

const unsigned char *buffer_read_peek(const struct buffer *b, size_t size) {
    const unsigned char *p = NULL;
    if (buffer_size(b) >= size) {
        p = &b->data[b->read_pos];
    }
    return p;
}
