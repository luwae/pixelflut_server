#include <stdio.h> 
#include <netdb.h> 
#include <netinet/in.h> 
#include <stdlib.h> 
#include <string.h> 
#include <sys/socket.h> 
#include <sys/types.h> 
#include <unistd.h> // read(), write(), close()
#define PORT 1337
#define SA struct sockaddr 

struct connection {
    int fd;
    struct sockaddr_in addr;
};

const char *msg = "hello!\n";

void connection_do(struct connection *conn) {
    ssize_t n = write(conn->fd, msg, 7);
    if (n != 7) {
        printf("write error\n");
        exit(0);
    }
}

void connection_close(struct connection *conn) {
    close(conn->fd);
}

// Driver function 
int main() 
{ 
	// socket create and verification 
	int sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) { 
		printf("socket creation failed...\n"); 
		exit(0); 
	} 

	// assign IP, PORT 
    struct sockaddr_in servaddr = {0};
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
	servaddr.sin_port = htons(PORT); 

	// Binding newly created socket to given IP and verification 
	if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) { 
		printf("socket bind failed...\n"); 
		exit(0); 
	} 

	// Now server is ready to listen and verification 
	if ((listen(sockfd, 5)) != 0) { 
		printf("Listen failed...\n"); 
		exit(0); 
	} 

    struct connection client;
	// Accept the data packet from client and verification 
    socklen_t len = sizeof(client.addr);
	client.fd = accept(sockfd, (SA*)&client.addr, &len); 
	if (client.fd < 0) { 
		printf("server accept failed...\n"); 
		exit(0); 
	} else {
		printf("server accept successful\n"); 
    }

	// Function for chatting between client and server 
	connection_do(&client);

	// After chatting close the socket 
	connection_close(&client); 
}

