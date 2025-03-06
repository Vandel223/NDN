#ifndef NODE_H
#define NODE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#define MAX_INTR 20
#define IP_LEN 16
#define PORT_LEN 6

typedef struct __Node Node;

typedef struct __Node_Addr {
    char ip[IP_LEN];
    char port[PORT_LEN];
} Node_Addr;

/**
 * @brief Creates a new Node.
 * 
 * @return Node* Pointer to the newly created Node.
 */
Node *node_create(void);

/**
 * @brief Sets the external address of the node.
 * 
 * @param node Pointer to the Node.
 * @param ext The external address to be set.
 */
void node_set_ext(Node *node, const Node_Addr ext);

/**
 * @brief Sets the safe address of the node.
 * 
 * @param node Pointer to the Node.
 * @param safe The safe address to be set.
 */
void node_set_safe(Node *node, const Node_Addr safe);

/**
 * @brief Increments the internal address list of the node.
 * 
 * @param node Pointer to the Node.
 * @param intr The internal address to be added.
 */
void node_incr_intr(Node *node, const Node_Addr intr);

/**
 * @brief Deletes an internal node from the list of internal nodes of the input node.
 *
 * The function searches for the input internal node, defined by its Node_Addr, (const Node_Addr intr) in the
 * list of internal nodes of the input node, passed as a pointer (Node *node). It then deletes it, and shifts
 * all the subsequent internal nodes one position to the left, so to rearrange the list.
 * The input node's internal node list length is also decremented.
 *
 * @param node The node from which the internal node will be deleted.
 * @param intr The internal node to be deleted.
 */
void node_del_intr(Node *node, const Node_Addr intr);

/**
 * @brief Frees the memory allocated for a Node.
 *
 * This function takes a pointer to a Node and frees the memory
 * that was allocated for it. It is important to ensure that the
 * Node pointer passed to this function was dynamically allocated
 * and is not NULL to avoid undefined behavior.
 *
 * @param node A pointer to the Node to be destroyed.
 */
void node_destroy(Node *node);

/**
 * @brief Prints the details of a given node.
 *
 * This function prints the external, safe, and internal IP addresses and ports of the specified node.
 * The external and safe addresses are printed in the format "IP:port".
 * The internal addresses are printed in a space-separated list in the format "IP:port".
 *
 * @param node A pointer to the Node structure whose details are to be printed.
 */
void print_node(Node *node);

#endif // NODE_H