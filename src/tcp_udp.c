#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200112L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <errno.h>
#include <time.h>


/**
 * @brief Prints an error message and exits the program.
 * 
 * @param msg The error message to be printed.
 */
static void error(const char *msg) {
    perror(msg);
    exit(1);
}

// TCP server socket
int create_tcp_server_socket(const char *port) {
    int server_fd;
    struct addrinfo hints, *res;
    int errcode;


    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;          // IPv4
    hints.ai_socktype = SOCK_STREAM;    // TCP
    hints.ai_flags = AI_PASSIVE;        // Server

    // listen TCP socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        error("ERROR: socket falhou");
    }
    // get own address info
    errcode = getaddrinfo(NULL, port, &hints, &res);
    if (errcode != 0) {
        error("ERROR: getaddrinfo falhou");
    }
    // bind TCP socket
    errcode = bind(server_fd, res->ai_addr, res->ai_addrlen);
    if (errcode == -1) {
        error("ERROR: bind falhou");
    }
    // start listening
    errcode = listen(server_fd, 5);
    if (errcode == -1) {
        error("ERROR: listen falhou");
    }

    freeaddrinfo(res);
    return server_fd;
}

// TCP client socket
int create_tcp_client_socket(const char *ip, const char *port) {
    int client_fd;
    struct addrinfo hints, *res;
    int errcode;

    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client_fd == -1) {
        error("ERROR: socket falhou");
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;          // IPv4
    hints.ai_socktype = SOCK_STREAM;    // TCP

    // get external neighbor's address info
    errcode = getaddrinfo(ip, port, &hints, &res);
    if (errcode != 0) {
        error("ERROR: getaddrinfo falhou");
    }
    // connect to neighbor
    errcode = connect(client_fd, res->ai_addr, res->ai_addrlen);
    if (errcode == -1) {
        error("ERROR: connect falhou");
    }

    freeaddrinfo(res);
    return client_fd;
}

// TCP send
void tcp_send(int sockfd, const char *message, int message_len) {
    int n = write(sockfd, message, message_len);
    if (n == -1) {
        error("ERROR: write falhou");
    }
}

// TCP receive
int tcp_receive(int sockfd, char *buffer, int buffer_size) {
    int n = read(sockfd, buffer, buffer_size - 1);
    if (n == -1) {
        error("ERROR: read falhou");
    }
    buffer[n] = '\0';

    return n;
}

// UDP create the socket
int create_udp_client_socket() {
    int sockfd;
    // timeout timer
    struct timeval timeout = {.tv_sec = 5, .tv_usec = 0};

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        error("ERROR: socket UDP falhou");
    }

    // socket options
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        error("ERROR: setsockopt falhou");
    }

    return sockfd;
}

// UDP get server info
struct addrinfo *get_udp_server_info(const char *server_ip, const char *server_port) {

    struct addrinfo hints, *res;
    int errcode;

    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_DGRAM;    //UDP socket
    hints.ai_family = AF_INET;         //IPv4

    // get address info on node server
    errcode = getaddrinfo(server_ip, server_port, &hints, &res);
        if(errcode != 0) error("ERROR: getaddrinfo falhou");    //Error

    return res;

}

// UDP send
void udp_send(int sockfd, struct addrinfo *res, const char *message, int message_len) {

    int n = sendto(sockfd, message, message_len, 0, res->ai_addr, res->ai_addrlen);
        if( n == -1)  error("ERROR: sendto falhou");   //Error
}

// UDP receive
void udp_receive(int sockfd, char *buffer, int buffer_size) {

    int n;
    int num_timeouts = 0;

    while ((n = recvfrom(sockfd, buffer, buffer_size - 1, 0, NULL, NULL)) == -1 && num_timeouts < 5) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            // Some unexpected error happened.
            error("ERROR: recvfrom falhou");
        }
        num_timeouts++;
        // Otherwise it was a timeout, just continue trying.
    }

    if (num_timeouts == 5) {
        error("ERROR: recvfrom deu timeout");
    }

    buffer[n] = '\0';
}