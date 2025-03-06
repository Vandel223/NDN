#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "../inc/node.h"

struct __Node {
    Node_Addr ext;
    Node_Addr safe;
    Node_Addr intr[MAX_INTR];
    int intr_len;
};

/**
 * @brief Prints an error message and exits the program.
 * 
 * @param msg The error message to be printed.
 */
static void error(const char *msg) {
    perror(msg);
    exit(1);
}

Node *node_create() {
    Node *node = (Node *) malloc(sizeof(Node));

    if (node == NULL) {
        error("ERROR: Erro ao criar nó");
    }

    node->intr_len = 0;
    return node;
}

void node_set_ext(Node *node, const Node_Addr ext) {
    // talvez copiar o conteudo de ext para node->ext
    strncpy(node->ext.ip, ext.ip, IP_LEN);
    strncpy(node->ext.port, ext.port, PORT_LEN);
}

Node_Addr node_get_ext(Node *node) {
    return node->ext;
}

int is_node_ext_own(Node *node) {
    return (strncmp(node->ext.ip, node->safe.ip, IP_LEN) == 0 && strncmp(node->ext.port, node->safe.port, PORT_LEN) == 0);
}

void node_set_safe(Node *node, const Node_Addr safe) {
    // talvez copiar o conteudo de safe para node->safe
    strncpy(node->safe.ip, safe.ip, IP_LEN);
    strncpy(node->safe.port, safe.port, PORT_LEN);
}

void node_incr_intr(Node *node, const Node_Addr intr) {
    if (node->intr_len == MAX_INTR) {
        error("ERROR: Número máximo de nós internos atingido");
    }
    // talvez copiar o conteudo de intr para node->intr
    strncpy(node->intr[node->intr_len].ip, intr.ip, IP_LEN);
    strncpy(node->intr[node->intr_len].port, intr.port, PORT_LEN);
    node->intr_len++;
}

void node_del_intr(Node *node, const Node_Addr intr) {
    int i;
    for (i = 0; i < node->intr_len; i++) {
        if (strncmp(node->intr[i].ip, intr.ip, IP_LEN) && node->intr[i].port == intr.port) {
            break;
        }
    }

    if (i == node->intr_len) {
        error("ERROR: Nó interno não encontrado");
    }

    for (; i < node->intr_len - 1; i++) {
        node->intr[i] = node->intr[i + 1];
    }

    node->intr_len--;
}

void node_destroy(Node *node) {
    free(node);
}

void print_node(Node *node) {
    if (node->ext.ip[0] == '\0')
        printf("External: NULL\n");
    else
        printf("External: %s:%s\n", node->ext.ip, node->ext.port);

    printf("Safe: %s:%s\n", node->safe.ip, node->safe.port);

    if (node->intr_len == 0) {
        printf("Internal: NULL\n");
        return;
    }
    
    printf("Internal: ");
    for (int i = 0; i < node->intr_len; i++) {
        printf("%s:%s ", node->intr[i].ip, node->intr[i].port);
    }
    printf("\n");
}