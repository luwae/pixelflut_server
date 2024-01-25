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

// conns[0..num_conns] contains the active connections.
// if one connection in the middle is removed, conns[num_conns - 1] is moved in its spot.
// this is like rust's Vec::swap_remove.
// when iterating over the connections, we need to make sure that the one that got swapped is not skipped.
#define MAX_CONNS 1024
struct connection conns[MAX_CONNS];
size_t num_conns = 0;
pthread_t net_thread;
volatile int should_quit = 0;

static void handle_new_connection(int sockfd) {
    if (num_conns == MAX_CONNS) {
        printf("WARNING: all connections occupied!\n"); // TODO
    } else {
        int connfd;
        struct sockaddr_in connaddr;
        socklen_t connlen = sizeof(connaddr);
        connfd = accept(sockfd, (struct sockaddr *) &connaddr, &connlen);
        if (IS_REAL_ERROR(connfd)) {
            perror("accept");
            exit(1); // TODO
        } else if (connfd != -1) { // we found a new connection (no error and no wouldblock)
            struct connection *c = &conns[num_conns];
            connection_init(c, connfd, connaddr);
            num_conns += 1;

            printf("accept ");
            connection_print(c);
        }
    }
}

static void close_and_swap(struct connection *c, const char *msg_prefix) {
    printf("%s ", msg_prefix);
    connection_print(c);

    connection_close(c);

    // move highest connection to this spot (no inactive connections between the active ones)
    struct connection *last = &conns[num_conns - 1];
    if (c != last) {
        memcpy(c, last, sizeof(*c));
    }
    num_conns -= 1;
}

static void *net_thread_main(void *arg) {
    int sockfd = (int)(intptr_t)arg;

    while (!should_quit) {
        handle_new_connection(sockfd);

        // move through all connections.
        // in each iteration, exactly one command of each connection is serviced.
        // every connection is allowed one read() and one write() syscall per loop iteration.
        // write() is only issued if we need to read bytes for the next command.
        // read() is only issued if there are bytes to be sent to the client.
        //
        // TODO instead of servicing one command, just service whatever's in the buffer?
        struct pixel px;
        for (size_t i = 0; i < num_conns; i++) {
            struct connection *c = &conns[i];
            if (c->fd == -1) {
                printf("connection not used?\n");
                exit(1); // TODO
            }
            // the server only handles new commands from a client if every possible command can be serviced without waiting.
            // here, this means that the sendbuf must have enough space for a GET command response.
            if (buffer_write_space(&c->sendbuf) < 4) { // NOTE: this should be the correct value, because the buffer should
                                                       // be moved to front here. TODO check
                goto do_send;
            }
            // this also means that the multisend must be empty
            if (!rect_iter_done(&c->multisend)) {
                goto do_send;
            }

            int status = connection_recv(c, &px);
            if (status == COMMAND_MULTIRECV_NEXT || status == COMMAND_PRINT) {
                canvas_set_px(&px);
            } else if (status == COMMAND_GET) {
                int inside_canvas = canvas_get_px(&px);
                // put pixel data in sendbuffer. We already know we have enough space.
                unsigned char *p = buffer_write_reserve(&c->sendbuf, 4);
                if (p == NULL) {
                    printf("not enough space in sendbuf?\n");
                    exit(1);
                }
                p[0] = px.r;
                p[1] = px.g;
                p[2] = px.b;
                p[3] = inside_canvas;
            } else if (status == COMMAND_MULTIRECV || status == COMMAND_MULTISEND) {
                // multirecv: do nothing. reading pixels starts in next loop iteration.
                // multisend: do nothing. sending pixels starts at do_send.
            } else if (status == COMMAND_FAULTY || status == COMMAND_WOULDBLOCK) {
                // do nothing
            } else if (status == COMMAND_CONNECTION_END) {
                close_and_swap(c, "close");
                i -= 1; // connection at this index is now another one
                continue;
            } else if (status == COMMAND_SYS_ERROR) {
                close_and_swap(c, "read() syscall error in");
                i -= 1; // connection at this index is now another one
                continue;
            } else {
                printf("wtf");
                exit(1);
            }

do_send:
            // fill from multisend, if remaining
            unsigned char *p;
            while (!rect_iter_done(&c->multisend) && (p = buffer_write_reserve(&c->sendbuf, 4)) != NULL) {
                px.x = c->multisend.x;
                px.y = c->multisend.y;
                rect_iter_advance(&c->multisend);
                int inside_canvas = canvas_get_px(&px);
                p[0] = px.r;
                p[1] = px.g;
                p[2] = px.b;
                p[3] = inside_canvas;
            }
            // try writing the sendbuffer (nonblock)
            // this only happens if the connection is not closed yet
            if (buffer_size(&c->sendbuf) > 0) {
                status = buffer_write_syscall(&c->sendbuf, c->fd);
                if (IS_REAL_ERROR(status)) {
                    close_and_swap(c, "write() syscall error in");
                    i -= 1; // connection at this index is now another one
                    continue;
                }
            }
        }
    }

    printf("closing network\n");
    for (size_t i = 0; i < num_conns; i++) {
        if (conns[i].fd != -1) {
            connection_close(&conns[i]);
        } else {
            printf("connection not used?\n");
            exit(1); // TODO
        }
    }
    close(sockfd);
    return NULL;
}

void net_start(void) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(1);
    }

    /* make the address immediatly reusable after the listening socket will have
     * been closed */
    int should_reuse_address = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &should_reuse_address,
        sizeof(should_reuse_address))) {
        perror("setsockopt");
        exit(1);
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
