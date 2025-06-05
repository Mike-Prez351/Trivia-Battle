/*******************************************************************************
 * Name        : server.c
 * Author      : Michael Preziosi
 * Pledge      : I pledge my honor that I have abided by the Stevens Honor System.
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>

struct Entry {
    char prompt[1024];
    char options[3][50];
    int answer_idx;
};

struct Player {
    int fd;
    int score;
    char name[128];
};

int read_questions(struct Entry* arr, char* filename) {
    FILE* fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("Error opening file");
        return -1;
    }

    int qcounter = 0;
    char line[1024];
    while (fgets(line, sizeof(line), fp) && qcounter < 50) {
        if (line[0] == '\n') continue;
        line[strcspn(line, "\n")] = 0;
        strcpy(arr[qcounter].prompt, line);
        
        if (!fgets(line, sizeof(line), fp)) break;
        char opt1[50], opt2[50], opt3[50];
        sscanf(line, "%s %s %s", opt1, opt2, opt3);
        strcpy(arr[qcounter].options[0], opt1);
        strcpy(arr[qcounter].options[1], opt2);
        strcpy(arr[qcounter].options[2], opt3);

        if (!fgets(line, sizeof(line), fp)) break;
        line[strcspn(line, "\n")] = 0;
        for (int i = 0; i < 3; i++) {
            if (strcmp(arr[qcounter].options[i], line) == 0) {
                arr[qcounter].answer_idx = i;
                break;
            }
        }
        
        fgets(line, sizeof(line), fp);
        qcounter++;
    }
    fclose(fp);
    return qcounter;
}

int main(int argc, char *argv[]) {
    char* question_file = "questions.txt";
    char* IP_address = "127.0.0.1";
    int port_number = 25555;

    int opt;
    while ((opt = getopt(argc, argv, "f:i:p:h")) != -1) {
        switch (opt) {
            case 'f':
                question_file = optarg;
                break;
            case 'i':
                IP_address = optarg;
                break;
            case 'p':
                port_number = atoi(optarg);
                break;
            case 'h':
                printf("Usage: %s [-f question_file] [-i IP_address] [-p port_number]\n\n", argv[0]);
                printf("  -f question_file      Default is \"qshort.txt\";\n");
                printf("  -i IP_address         Default is \"127.0.0.1\";\n");
                printf("  -p port_number        Default is 25555;\n");
                printf("  -h                    Display this help info.\n");
                exit(EXIT_SUCCESS);
            case '?':
                fprintf(stderr, "Error: Unknown option '-%c' received.\n", optopt);
                exit(EXIT_FAILURE);
        }
    }

    struct Entry questions[50];
    int qcounter = read_questions(questions, question_file);
    if (qcounter < 0) {
        fprintf(stderr, "Error reading questions from %s.\n", question_file);
        exit(EXIT_FAILURE);
    }

    int serverfd;
    struct sockaddr_in serveraddr;
    
    serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverfd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = inet_addr(IP_address);
    serveraddr.sin_port = htons(port_number);

    if (bind(serverfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(serverfd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    printf("Welcome to 392 Trivia!\n");

    struct Player players[3];
    int pcounter = 0;

    while (pcounter < 3) {
        struct sockaddr_in clientaddr;
        socklen_t addrlen = sizeof(clientaddr);

        int clientfd = accept(serverfd, (struct sockaddr*)&clientaddr, &addrlen);
        if (clientfd < 0) {
            perror("accept");
            continue;
        }
        if (pcounter >= 3) {
            printf("Max connection reached!\n");
            close(clientfd);
            continue;
        }

        printf("New connection detected!\n");
        
        char buffer[128] = "Please type your name:\n";
        send(clientfd, buffer, strlen(buffer), 0);
        memset(buffer, 0, sizeof(buffer));
        
        int bytes = recv(clientfd, buffer, sizeof(buffer)-1, 0);
        if (bytes <= 0) {
            printf("Lost connection!\n");
            close(clientfd);
            continue;
        }

        buffer[strcspn(buffer, "\r\n")] = 0;
        printf("Hi %s!\n", buffer);

        players[pcounter].fd = clientfd;
        players[pcounter].score = 0;
        strncpy(players[pcounter].name, buffer, sizeof(players[pcounter].name)-1);
        pcounter++;
    }

    printf("The game starts now!\n");
    for (int i = 0; i < qcounter; i++) {
        struct Entry* currquestion = &questions[i];

        printf("Question %d: %s\n", i+1, currquestion->prompt);
        for (int j = 0; j < 3; j++) {
            printf("%d: %s\n", j+1, currquestion->options[j]);
        }

        char message[1024];
        for (int j = 0; j < 3; j++) {
            send(players[j].fd, "Question ", 9, 0);
            sprintf(message, "%d: ", i+1);
            send(players[j].fd, message, strlen(message), 0);
            send(players[j].fd, currquestion->prompt, strlen(currquestion->prompt), 0);
            send(players[j].fd, "\n", 1, 0);
        
            send(players[j].fd, "Press 1: ", 9, 0);
            send(players[j].fd, currquestion->options[0], strlen(currquestion->options[0]), 0);
            send(players[j].fd, "\n", 1, 0);
        
            send(players[j].fd, "Press 2: ", 9, 0);
            send(players[j].fd, currquestion->options[1], strlen(currquestion->options[1]), 0);
            send(players[j].fd, "\n", 1, 0);
        
            send(players[j].fd, "Press 3: ", 9, 0);
            send(players[j].fd, currquestion->options[2], strlen(currquestion->options[2]), 0);
            send(players[j].fd, "\n", 1, 0);
        }

        fd_set readfds;
        int maxfd = -1;
        FD_ZERO(&readfds);
        for (int j = 0; j < 3; j++) {
            FD_SET(players[j].fd, &readfds);
            if (players[j].fd > maxfd) maxfd = players[j].fd;
        }

        if (select(maxfd+1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        char buff[128];
        int whoanswered = -1;
        int response = -1;

        for (int j = 0; j < 3; j++) {
            if (FD_ISSET(players[j].fd, &readfds)) {
                int n = recv(players[j].fd, buff, sizeof(buff)-1, 0);
                if (n <= 0) {
                    printf("Lost connection!\n");
                    close(players[j].fd);
                    continue;
                }

                buff[n] = 0;
                response = atoi(buff)-1;
                if (response < 0 || response >= 3) {
                    printf("Invalid response received.\n");
                    continue;
                }
                whoanswered = j;

                if (response == currquestion->answer_idx) {
                    players[j].score++;
                } else {
                    players[j].score--;
                }

                break;
            }
        }

        for (int j = 0; j < 3; j++) {
            send(players[j].fd, "Correct answer: ", 16, 0);
            send(players[j].fd, currquestion->options[currquestion->answer_idx],
                 strlen(currquestion->options[currquestion->answer_idx]), 0);
            send(players[j].fd, "\n", 1, 0);
        }

        sleep(1);
    }

    int winnerid = 0;
    for (int j = 1; j < 3; j++) {
        if (players[j].score > players[winnerid].score)
            winnerid = j;
    }

    printf("Congrats, %s!\n", players[winnerid].name);

    for (int j = 0; j < 3; j++) {
        close(players[j].fd);
    }
    
    close(serverfd);
    return 0;
}