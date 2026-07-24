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
    int client_fd;
    struct sockaddr_in my_addr;
    int status;
    char recv_buf[MAX_MESSAGE_LEN];
    char send_buf[MAX_MESSAGE_LEN];
    struct pollfd pfds[PFDS_LEN];
    
    client_fd = socket(PF_INET, SOCK_STREAM, 0);

    if (client_fd == -1) {
        perror("socket");
        exit(1);
    }

    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(8080);
    my_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    memset(my_addr.sin_zero, '\0', sizeof my_addr.sin_zero);

    status = connect(client_fd, (struct sockaddr *)&my_addr, sizeof my_addr);
    
    if (status < 0) {
        perror("Connection failed");
        exit(1);
    }

    pfds[0].fd = STDIN_FILENO;
    pfds[0].events = POLLIN;
    pfds[1].fd = client_fd;
    pfds[1].events = POLLIN;

    printf("Connection established!\nYou are now chatting. Type '/exit' to quit.\n");

    while (1) {
        int conn;
        int poll_count;
        char *send_buf_status;

        poll_count = poll(pfds, 2, -1);

        printf("> ");

        if (poll_count > 0) {
            if (pfds[1].revents & POLLIN) {
                conn = recv(client_fd, recv_buf, MAX_MESSAGE_LEN - 1, 0);

                if (conn == 0) {  //only check conn if you actually set it
                    printf("Lost connection to the host.\n");
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
    }

    close(client_fd);
}
