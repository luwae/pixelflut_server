// A simple bounded byte-buffer.
// The buffer keeps all its bytes in contiguous memory. There is no wrap-around.
#ifndef PFS_BUFFER_H
#define PFS_BUFFER_H

#include <stddef.h>

struct buffer {
    size_t read_pos;
    size_t write_pos;
    size_t capacity;
    unsigned char *data;
};

// allocate space for buffer (only backing space of size `capacity`, not space for the struct).
void buffer_init_malloc(struct buffer *b, size_t capacity);
void buffer_destroy_malloc(struct buffer *b);

// how many bytes in the buffer to consume.
size_t buffer_size(const struct buffer *b);

// how much free space in the buffer.
size_t buffer_write_space(const struct buffer *b);

// move buffer data to front, creating contiguous space in the back.
void buffer_move_front(struct buffer *b);

// do a single `read` syscall and try to fill the buffer as much as possible.
// (note that this writes to the buffer, counter to the name of this function).
// Panics if the buffer is already full.
int buffer_read_syscall(struct buffer *b, int fd);

// do a single `write` syscall and try to empty the buffer as much as possible.
// (note that this reads from the buffer, counter to the name of this function).
// Panics if the buffer is already empty.
int buffer_write_syscall(struct buffer *b, int fd);

// Tries to reserve `size` bytes to read from the buffer, by moving the read position.
// After the call, these bytes can be read from the returned address.
// May return `NULL` if not enough bytes are present.
const unsigned char *buffer_read_reserve(struct buffer *b, size_t size);

// Tries to reserve `size` bytes in the buffer to write to, by moving the write position.
// After the call, these bytes can be written to the returned address.
// May return `NULL` if not enough space is present.
unsigned char *buffer_write_reserve(struct buffer *b, size_t size);

// Returns pointer to the next `size` bytes in the buffer, without modifying read position.
// May retur `NULL` if not enough bytes are present.
const unsigned char *buffer_read_peek(const struct buffer *b, size_t size);

#endif
