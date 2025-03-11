#ifndef NDN_H
#define NDN_H

#define _POSIX_C_SOURCE 200112L

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

#include "../inc/node.h"

#define UDP_BUFF_SIZE 256
#define TCP_BUFF_SIZE 256
#define STDIN_BUFF_SIZE 256
#define OPTIONS_BUFF_SIZE 256
#define MAX_NODES 20

#define STDIN 0

#define UDP_PORT 58007

#define max(a, b) ((a) > (b) ? (a) : (b))

#endif