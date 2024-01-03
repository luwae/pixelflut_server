#ifndef PFS_COMMON_H
#define PFS_COMMON_H

struct pixel {
    unsigned int x;
    unsigned int y;
    unsigned char r;
    unsigned char g;
    unsigned char b;
};

#define WOULD_BLOCK(ret) ((ret) == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))
#define IS_REAL_ERROR(ret) ((ret) == -1 && errno != EAGAIN && errno != EWOULDBLOCK)

// signal for threads to exit
extern volatile int should_quit;

#endif
