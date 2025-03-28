#ifndef NDN_H
#define NDN_H

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

#include "../inc/node_addr.h"
#include "../inc/tcp_udp.h"

#define UDP_BUFF_SIZE 256
#define TCP_BUFF_SIZE 256
#define STDIN_BUFF_SIZE 256
#define OPTIONS_BUFF_SIZE 256
#define MAX_NODES 20
#define NAME_BUFF_SIZE 101 // name é 100 caracteres máximo + '\0'
#define MAX_NEIGH 20
#define MAX_INTEREST 20
#define MAX_OBJ 20

#define STDIN 0

#define UDP_PORT 58007

#define IP_LEN 16
#define PORT_LEN 6

#define UDP_SV_IP "193.136.138.142"
#define UDP_SV_PORT "59000"

#define max(a, b) ((a) > (b) ? (a) : (b))

typedef enum __State {
    CLOSE = 0,
    WAIT,
    ANSWER,
 } State;

 typedef struct __State_FD {
    int fd;
    State state;
 } State_FD;

 typedef struct __Interest {
    char name[NAME_BUFF_SIZE];
    State_FD state_fd[MAX_NEIGH];
    int state_fd_len;
 } Interest;

#endif