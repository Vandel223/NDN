#ifndef TCP_UDP_H
#define TCP_UDP_H

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

// ...existing code...

// TCP connection functions

/**
 * @brief Creates a TCP server socket.
 * 
 * @param port The port number to bind the server socket to.
 * @return int The file descriptor of the created server socket.
 */
int create_tcp_server_socket(const char *port);

/**
 * @brief Creates a TCP client socket and connects to the specified server.
 * 
 * @param ip The IP address of the server to connect to.
 * @param port The port number of the server to connect to.
 * @return int The file descriptor of the created client socket.
 */
int create_tcp_client_socket(const char *ip, const char *port);

/**
 * @brief Sends a message over a TCP connection.
 * 
 * @param sockfd The file descriptor of the TCP socket.
 * @param message The message to be sent.
 * @param message_len The length of the message to be sent.
 */
void tcp_send(int sockfd, const char *message, int message_len);

/**
 * @brief Receives a message over a TCP connection.
 * 
 * @param sockfd The file descriptor of the TCP socket.
 * @param buffer The buffer to store the received message.
 * @param buffer_size The size of the buffer.
 * @return int The return value of read().
 */
int tcp_receive(int sockfd, char *buffer, int buffer_size);

// UDP connection functions

/**
 * @brief Creates a UDP client socket.
 * 
 * @return int The file descriptor of the created client socket.
 */
int create_udp_client_socket(void);

/**
 * @brief Gets the address information of the UDP server.
 * 
 * @param server_ip The IP address of the server.
 * @param server_port The port number of the server.
 * @param res Pointer to the addrinfo structure to store the result.
 * @return struct addrinfo * The pointer to the addrinfo, allocated.
 */
struct addrinfo *get_udp_server_info(const char *server_ip, const char *server_port);

/**
 * @brief Sends a message to the UDP server.
 * 
 * @param sockfd The file descriptor of the UDP socket.
 * @param res The addrinfo structure containing the server address information.
 * @param message The message to be sent.
 * @param message_len The length of the message to be sent.
 */
void udp_send(int sockfd, struct addrinfo *res, const char *message, int message_len);

/**
 * @brief Receives a message from the UDP server.
 * 
 * @param sockfd The file descriptor of the UDP socket.
 * @param buffer The buffer to store the received message.
 * @param buffer_size The size of the buffer.
 */
void udp_receive(int sockfd, char *buffer, int buffer_size);

#endif // TCP_UDP_H
