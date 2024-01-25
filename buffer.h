#ifndef PFS_BUFFER_H
#define PFS_BUFFER_H

#define CONN_BUF_SIZE 1024
struct buffer {
    size_t read_pos;
    size_t write_pos;
    unsigned char *data;
};

void buffer_init_malloc(struct buffer *b);
void buffer_destroy_malloc(struct buffer *b);
size_t buffer_size(const struct buffer *b);
size_t buffer_write_space(const struct buffer *b);
void buffer_move_front(struct buffer *b);
int buffer_read_syscall(struct buffer *b, int fd);
int buffer_write_syscall(struct buffer *b, int fd);
const unsigned char *buffer_read_reserve(struct buffer *b, size_t size);
unsigned char *buffer_write_reserve(struct buffer *b, size_t size);

#endif
