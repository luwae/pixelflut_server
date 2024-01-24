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
            // we only read a new command from the client if we can incorporate it.
            // case 1: sendbuffer is full -> we can't incorporate another GET
            if (buffer_size(&c->sendbuf) > CONN_BUF_SIZE - 4) {
               goto do_send;
            }
            // case 2: multisend is not done -> we can't incorporate another multisend
            if (!rect_iter_done(&c->multisend)) {
                goto do_send;
            }

            int status = connection_recv(c, &px);
            if (status == COMMAND_FAULTY) {
                // do nothing
            } else if (status == COMMAND_PRINT || status == COMMAND_MULTIRECV) {
                canvas_set_px(&px);
            } else if (status == COMMAND_GET) {
                int inside_canvas = canvas_get_px(&px);
                // put pixel data in sendbuffer. We already know we have enough space.
                // If coordinates are outside range, send (0,0,0)
                // we can't send nothing because the client expects pixel data,
                // and it might be easier to avoid edge conditions
                // TODO perhaps a flag if it was inside or outside range, but then
                // 4 bytes won't be enough anymore
                unsigned char *sp = buffer_write_reserve(&c->sendbuf, 4);
                if (sp == NULL) {
                    printf("wtf\n");
                    exit(1);
                }
                sp[0] = px.r;
                sp[1] = px.g;
                sp[2] = px.b;
                sp[3] = inside_canvas;
            } else if (status == COMMAND_MULTISEND) {
                goto do_send;
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

do_send:
            buffer_move_front(&c->sendbuf);
            unsigned char *sp;
            while (!rect_iter_done(&c->multisend)
                    && (sp = buffer_write_reserve(&c->sendbuf, 4)) != NULL)
            {
                px.x = c->multisend.x;
                px.y = c->multisend.y;
                rect_iter_advance(&c->multisend);
                int inside_canvas = canvas_get_px(&px);
                sp[0] = px.r;
                sp[1] = px.g;
                sp[2] = px.b;
                sp[3] = inside_canvas;
            }

            // try writing the sendbuffer (nonblock)
            // this only happens if the connection is not closed yet
            if (buffer_size(&c->sendbuf) > 0) {
                status = buffer_send_nonblocking(&c->sendbuf, c->fd);
                if (IS_REAL_ERROR(status)) {
                    perror("write");
                    exit(1); // TODO
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
