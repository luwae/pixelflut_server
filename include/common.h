#ifndef PFS_COMMON_H
#define PFS_COMMON_H

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

struct pixel {
    unsigned int x;
    unsigned int y;
    unsigned char r;
    unsigned char g;
    unsigned char b;
};

#define PANIC(msg) do { \
    printf("PANIC@%s:%d: %s\n", __FILE__, __LINE__, msg); \
    exit(1); \
} while (0)

#define WOULD_BLOCK(ret) ((ret) == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
#define IS_REAL_ERROR(ret) ((ret) == -1 && errno != EAGAIN && errno != EWOULDBLOCK)

#endif
