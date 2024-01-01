#include <stdio.h> 
#include <netinet/in.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <sys/types.h> 
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "common.h"
#include "connection.h"

#define PORT 1337

#define MAX_CONNS 10
struct connection conns[MAX_CONNS];

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl");
        exit(1);
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl");
        exit(1);
    }
}

int main() 
{ 
    for (int i = 0; i < MAX_CONNS; i++) {
        conns[i].fd = -1;
    }

	// socket create and verification 
	int sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) { 
		perror("socket");
		exit(0);
	} 

	// assign IP, PORT 
    struct sockaddr_in servaddr = {0};
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
	servaddr.sin_port = htons(PORT); 

	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr))) != 0) { 
		perror("bind");
		exit(1);
	} 

	// Now server is ready to listen and verification 
	if ((listen(sockfd, 5)) != 0) { 
		perror("listen");
		exit(1);
	} 

    set_nonblocking(sockfd);

    while (1) {
        int free = -1;
        for (int i = 0; i < MAX_CONNS; i++) {
            if (conns[i].fd == -1) {
                free = i;
                break;
            }
        }
        if (free == -1) {
            printf("no free slot\n");
            exit(1);
        }
        struct connection *c = &conns[free];

        int connfd;
        struct sockaddr_in connaddr;
        socklen_t connlen = sizeof(connaddr);
        connfd = accept(sockfd, (struct sockaddr *) &connaddr, &connlen);
        if (IS_REAL_ERROR(connfd)) {
            perror("accept");
            exit(1);
        } else if (connfd != -1) {
            printf("accept connection ");
            in_addr_t a = connaddr.sin_addr.s_addr;
            printf("%d.%d.%d.%d\n", a & 0xff, (a >> 8) & 0xff, (a >> 16) & 0xff, (a >> 24) & 0xff);
            set_nonblocking(connfd);
            c->fd = connfd;
            c->addr = connaddr;
            c->read_pos = c->write_pos = 0;
        }

        // move through all connections
        struct pixel px;
        for (int i = 0; i < MAX_CONNS; i++) {
            if (conns[i].fd == -1)
                continue;
            int status = connection_get(&conns[i], &px);
            if (status == GET_SUCCESS) {
                // TODO draw px
                printf("Pixel { x: %u y: %u col: (%d, %d, %d) } from ", px.x, px.y, px.r, px.g, px.b);
                in_addr_t a = conns[i].addr.sin_addr.s_addr;
                printf("%d.%d.%d.%d\n", a & 0xff, (a >> 8) & 0xff, (a >> 16) & 0xff, (a >> 24) & 0xff);
            } else if (status == GET_WOULDBLOCK) {
                // do nothing
            } else { // if status == GET_CONNECTION_END
                printf("close connection ");
                in_addr_t a = conns[i].addr.sin_addr.s_addr;
                printf("%d.%d.%d.%d\n", a & 0xff, (a >> 8) & 0xff, (a >> 16) & 0xff, (a >> 24) & 0xff);
                connection_close(&conns[i]);
            }
        }
    }
}
