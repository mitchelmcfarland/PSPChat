#include <stdio.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <poll.h>

#define MAX_MESSAGE_LEN 4096
#define PFDS_LEN 2

int main () {
	//int yes = 1;
    int serv_fd;
    struct sockaddr_in my_addr;
	char send_buf[MAX_MESSAGE_LEN];
	char recv_buf[MAX_MESSAGE_LEN];
	struct pollfd pfds[PFDS_LEN];
    
    serv_fd = socket(PF_INET, SOCK_STREAM, 0);

	if (serv_fd == -1) {
		perror("socket");
		exit(1);
	}

    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(8080);
    my_addr.sin_addr.s_addr = INADDR_ANY;
    memset(my_addr.sin_zero, '\0', sizeof my_addr.sin_zero);

	if (bind(serv_fd, (struct sockaddr *)&my_addr, sizeof my_addr) == -1) {
		perror("bind");
		exit(1);
	}

	printf("Waiting for connection...\n");
	
	if (listen(serv_fd, 5) == -1) {
		perror("listen");
		exit(1);
	}

	struct sockaddr_in client_addr;
	socklen_t addr_size;
	int client_fd;

	addr_size = sizeof client_addr;

	client_fd = accept(serv_fd, (struct sockaddr *)&client_addr, &addr_size);

	if (client_fd == -1) {
		perror("accept");
		exit(1);
	}

    pfds[0].fd = STDIN_FILENO;
    pfds[0].events = POLLIN;
    pfds[1].fd = client_fd;
    pfds[1].events = POLLIN;

	printf("Connection established!\nYou are now chatting. Type '/exit' to quit.\n");

	while(1) {
		char *send_buf_status;
		int poll_count;
		int conn;

		poll_count = poll(pfds, 2, -1);

		printf("> ");

		if (poll_count > 0) {
            if (pfds[1].revents & POLLIN) {
                conn = recv(client_fd, recv_buf, MAX_MESSAGE_LEN - 1, 0);

                if (conn == 0) {  //only check conn if you actually set it
                    printf("Lost connection to the client.\n");
                    break;
                }

                printf("%s", recv_buf);
			}

			if (pfds[0].revents & POLLIN) {
                send_buf_status = fgets(send_buf, MAX_MESSAGE_LEN - 1, stdin);

                if (strncmp(send_buf, "/exit", 5) == 0 || send_buf_status == NULL) { //same thing, only check what fgets returns if you actually set it
                    printf("Exiting program...\n");
                    break;
                }

				send(client_fd, send_buf, MAX_MESSAGE_LEN - 1, 0);

				printf("Message sent.\n");

            }
		}

		//buffer[strcspn(buffer, "\n") = '\0'];

	}
	
	close(client_fd);

	close(serv_fd);

	return 0;
}