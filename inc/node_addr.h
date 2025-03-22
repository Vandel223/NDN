#ifndef NODE_ADDR_H
#define NODE_ADDR_H

#include <string.h>
#include <stdio.h>

#define IP_LEN 16
#define PORT_LEN 6

#define cmp_Node_Addr(Node_Addr0, Node_Addr1) ((strncmp(Node_Addr0.ip, Node_Addr1.ip, IP_LEN) == 0) && (strncmp(Node_Addr0.port, Node_Addr1.port, PORT_LEN) == 0))
#define cpy_Node_Addr(src_Node_Addr, dest_Node_Addr) (snprintf(src_Node_Addr.ip, IP_LEN, "%s", dest_Node_Addr.ip), snprintf(src_Node_Addr.port, PORT_LEN, "%s", dest_Node_Addr.port))

typedef struct __Node_Addr {
    char ip[IP_LEN];
    char port[PORT_LEN];
} Node_Addr;

typedef struct __Node_Addr_FD {
    Node_Addr node_addr;
    int fd;
} Node_Addr_FD;

#endif
