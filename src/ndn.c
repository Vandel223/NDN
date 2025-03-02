#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "../inc/node.h"
#include "../inc/ndn.h"

void main(int argc, char *argv[]) {

    Node *node = node_create();

    int sockfd, newsockfd;
    int errcode;
    struct sockaddr_in server_addr;
    struct addrinfo hints, *res;
    char buffer[TCP_BUFF_SIZE];

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("TCP socket creation failed");
        exit(EXIT_FAILURE);
    }

    if ((errcode = getaddrinfo("tejo.tecnico.ulisboa.pt", "59000", &hints, &res)) != 0) {
        perror("getaddrinfo failed");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("TCP connection failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    send(sockfd, message, strlen(message), 0);
    printf("TCP Node sent: %s\n", message);

    close(sockfd);
    return NULL;
}