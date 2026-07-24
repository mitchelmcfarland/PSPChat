#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>

int main () {
	int status;
	struct addrinfo hints;
	struct addrinfo *servinfo;

	memset(&hints, 0, sizeof hints);
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	
	if ((status = getaddrinfo(NULL, "8080", &hints, &servinfo)) != 0) {
		fprintf(stderr, "Gai error: %s\n", gai_strerror(status));
		exit(1);
	}

	int socket_fd;

	socket_fd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

	bind(socket_fd, servinfo->ai_addr, servinfo->ai_addrlen);
	
	listen(socket_fd, 5);

	struct sockaddr_storage client_addr;
	socklen_t addr_size;
	int client_fd;

	addr_size = sizeof client_addr;

	client_fd = accept(socket_fd, (struct sockaddr *)&client_addr, &addr_size);

	printf("Connection established!\n");

	close(client_fd);

	close(socket_fd);

	freeaddrinfo(servinfo);
}
