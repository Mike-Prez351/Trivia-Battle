/*******************************************************************************
 * Name        : client.c
 * Author      : Michael Preziosi
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>

void parseconnect(int argc, char** argv, int* serverfd) {
    char* IP_address = "127.0.0.1";
    int port_number = 25555;
    int opt;
    while ((opt = getopt(argc, argv, "i:p:h")) != -1) {
        switch (opt) {
            case 'i':
                IP_address = optarg;
                break;
            case 'p':
                port_number = atoi(optarg);
                break;
            case 'h':
                printf("Usage: %s [-i IP_address] [-p port_number] [-h]\n\n", argv[0]);
                printf("  -i IP_address         Default to \"127.0.0.1\";\n");
                printf("  -p port_number        Default to 25555;\n");
                printf("  -h                    Display this help info.\n");
                exit(EXIT_SUCCESS);
            case '?':
                fprintf(stderr, "Error: Unknown option '-%c' received.\n", optopt);
                exit(EXIT_FAILURE);
        }
    }

    *serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (*serverfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serveraddr;
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(IP_address);
    serveraddr.sin_port = htons(port_number);

    if (connect(*serverfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }
}

int main(int argc, char* argv[]) {
    int serverfd;
    parseconnect(argc, argv, &serverfd);
    fd_set readfds;
    char buffer[1024];
    int maxfd;
    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(serverfd, &readfds);

        maxfd = (STDIN_FILENO > serverfd) ? STDIN_FILENO : serverfd;

        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select");
            exit(EXIT_FAILURE);
        }

        if (FD_ISSET(serverfd, &readfds)) {
            memset(buffer, 0, sizeof(buffer));
            int bytes = recv(serverfd, buffer, sizeof(buffer)-1, 0);
            
            if (bytes <= 0) {
                printf("Server disconnected.\n");
                break;
            }
            
            buffer[bytes] = '\0';
            printf("%s", buffer);
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            memset(buffer, 0, sizeof(buffer));
            if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
                break;
            }
            
            if (send(serverfd, buffer, strlen(buffer), 0) < 0) {
                perror("send");
                exit(EXIT_FAILURE);
            }
        }
    }
    close(serverfd);
    return 0;
}
