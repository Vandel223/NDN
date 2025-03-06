#ifndef NDN_H
#define NDN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>

#include "../inc/node.h"

#define UDP_BUFF_SIZE 1024
#define TCP_BUFF_SIZE 1024
#define STDIN_BUFF_SIZE 1024

#define STDIN 0

#define UDP_PORT 58007

#define max(a, b) ((a) > (b) ? (a) : (b))

#endif