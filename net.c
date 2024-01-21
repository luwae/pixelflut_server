#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include "SDL.h"

#include "common.h"
#include "canvas.h"
#include "connection.h"
#include "net.h"

#define PORT 1337

#define MAX_CONNS 10
struct connection conns[MAX_CONNS];
pthread_t net_thread;
volatile int should_quit = 0;

static void *net_thread_main(void *arg) {
    int sockfd = (int)(intptr_t)arg;

    while (!should_quit) {
        int free = -1;
        for (int i = 0; i < MAX_CONNS; i++) {
            if (conns[i].fd == -1) {
                free = i;
                break;
            }
        }
        if (free == -1) {
            printf("no free slot\n");
            exit(1); // TODO
        }
        struct connection *c = &conns[free];

        int connfd;
        struct sockaddr_in connaddr;
        socklen_t connlen = sizeof(connaddr);
        connfd = accept(sockfd, (struct sockaddr *) &connaddr, &connlen);
        if (IS_REAL_ERROR(connfd)) {
            perror("accept");
            exit(1); // TODO
        } else if (connfd != -1) { // we found a new connection (no error and no wouldblock)
            connection_init(c, connfd, connaddr, free);
        }

        // move through all connections
        struct pixel px;
        for (int i = 0; i < MAX_CONNS; i++) {
            c = &conns[i];
            if (c->fd == -1)
                continue;
            // the client is only allowed to send a command to the server if it has
            // enough space in its send buffer for us to be able to incorporate the
            // command.
            int sendbuf_size = c->send_write_pos - c->send_read_pos;
            if (sendbuf_size == CONN_BUF_SIZE) {
                continue;
            }

            int status = connection_recv(c, &px);
            if (status == COMMAND_FAULTY) {
                // do nothing
            } else if (status == COMMAND_PRINT) {
                c->tracker.num_command_print += 1;
                if (!canvas_set_px(&px)) {
                    c->tracker.num_coords_outside_canvas += 1;
                }
            } else if (status == COMMAND_GET) {
                int inside_canvas = canvas_get_px(&px);
                c->tracker.num_command_get += 1;
                if (!inside_canvas)
                    c->tracker.num_coords_outside_canvas += 1;
                // put pixel data in sendbuffer. We already know we have enough space.
                // If coordinates are outside range, send (0,0,0)
                // we can't send nothing because the client expects pixel data,
                // and it might be easier to avoid edge conditions
                // TODO perhaps a flag if it was inside or outside range, but then
                // 4 bytes won't be enough anymore
                if (c->send_write_pos == CONN_BUF_SIZE) { // need to move the buffer
                    memmove(c->sendbuf, &c->sendbuf[c->send_read_pos], sendbuf_size);
                    c->send_read_pos = 0;
                    c->send_write_pos = sendbuf_size;
                }
                unsigned char *sp = &c->sendbuf[c->send_write_pos];
                c->send_write_pos += 4;
                sp[0] = px.r;
                sp[1] = px.g;
                sp[2] = px.b;
                sp[3] = inside_canvas;
            } else if (status == COMMAND_WOULDBLOCK) {
                // do nothing
            } else if (status == COMMAND_CONNECTION_END) {
                printf("close ");
                connection_print(c, i);

                connection_close(c);
                continue;
            } else {
                printf("wtf");
                exit(1);
            }

            // try writing the sendbuffer (nonblock)
            // this only happens if the connection is not closed yet
            sendbuf_size = c->send_write_pos - c->send_read_pos; // might have changed
            if (sendbuf_size > 0) {
                status = write(c->fd, &c->sendbuf[c->send_read_pos], sendbuf_size);
                c->tracker.num_write_syscalls += 1;
                if (IS_REAL_ERROR(status)) {
                    perror("write");
                    exit(1); // TODO
                } else if (WOULD_BLOCK(status)) {
                    c->tracker.num_write_syscalls_wouldblock += 1;
                }
                if (status > 0) {
                    c->send_read_pos += status;
                    c->tracker.num_bytes_sent_to_client += status;
                    if (status % 4) {
                        printf("WARNING: data sent to client not divisible by 4.\n");
                    }
                    c->tracker.num_pixels_sent_to_client += status / 4;
                }
            }
        }
    }

    printf("closing network\n");
    for (int i = 0; i < MAX_CONNS; i++) {
        if (conns[i].fd != -1) {
            connection_close(&conns[i]);
        }
    }
    close(sockfd);
    return NULL;
}

void net_start(void) {
    for (int i = 0; i < MAX_CONNS; i++) {
        conns[i].fd = -1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(0);
    }

    struct sockaddr_in servaddr = {0};
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    if (bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) != 0) {
        perror("bind");
        exit(1);
    }

    if (listen(sockfd, 5) != 0) {
        perror("listen");
        exit(1);
    }

    set_nonblocking(sockfd);

    if (pthread_create(&net_thread, NULL, net_thread_main, (void*)(intptr_t)sockfd) != 0) {
        printf("pthread_create\n");
        exit(1);
    }
}

void net_stop(void) {
    should_quit = 1;
    pthread_join(net_thread, NULL);
}
