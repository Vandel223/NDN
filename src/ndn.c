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
#include "../inc/ndn.h"
#include "../inc/tcp_udp.h"

/**
 * @brief Prints an error message and exits the program.
 * 
 * @param msg The error message to be printed.
 */
static void error(const char *msg) {
    perror(msg);
    exit(1);
}

/**
 * @brief Prints the help message for the program.
 *
 * This function displays the help message, which typically includes
 * information about the program's usage and available commands.
 * It is intended to guide users on how to properly use the program.
 */
void print_help(void) {
    fprintf(stdout,  "\n\n\tjoin (j) <net>\n"
                    "Entrada do nó na rede net. Os valores de net são representados por três dígitos,"
                    "podendo variar entre 000 e 999.\n\n"
                    "\tdirect join (dj) <connectIP> <connectTCP>\n"
                    "Entrada do nó ligando-se ao nó com identificador connectIP connectTCP, sem"
                    "registo no servidor de nós. Se connectIP for 0.0.0.0, então a rede é criada"
                    "com apenas o nó.\n\n"
                    "\tcreate (c) <name>\n"
                    "Criação de um objeto com nome name. Os valores de name são representados por"
                    "sequências alfanuméricas com um máximo de 100 carateres. Para simplificar,"
                    "cria-se apenas o nome do objeto, omitindo-se o objeto propriamente dito.\n\n"
                    "\tdelete (dl) <name>\n"
                    "Remoção do objeto com nome name.\n\n"
                    "\tretrieve (r) <name>\n"
                    "Pesquisa do objeto com nome name.\n\n"
                    "\tshow topology (st)\n"
                    "Visualização dos identificadores dos vizinhos externo, de salvaguarda e internos.\n\n"
                    "\tshow names (sn)\n"
                    "Visualização dos nomes de todos os objetos guardados no nó.\n\n"
                    "\tshow interest table (si)\n"
                    "Visualização de todas as entradas da tabela de interesses pendentes.\n\n"
                    "\tleave (l)\n"
                    "Saída do nó da rede.\n\n"
                    "\texit (x)\n"
                    "Fecho da aplicação.\n\n\n");
}

/**
 * @brief Prints the IP and port of a given node address.
 *
 * @param node A pointer to the Node structure whose details are to be printed.
 */
void print_node_addr(Node_Addr node_addr) {
    if (node_addr.ip[0] != '\0')
        printf("%s:%s", node_addr.ip, node_addr.port);
    else
        printf("NULL");
}

int main(int argc, char *argv[]) {

    // own node
    int created = 0;
    Node_Addr own_addr;
    Node_Addr safe_addr;
    Node_Addr ext_addr;
    Node_Addr_FD neigh_addr_fd[MAX_NEIGH];
        for (int i = 0; i < MAX_NEIGH; i++) {
            neigh_addr_fd[i].fd = 0;
            neigh_addr_fd[i].buffer_len = 0;
        }
    int neigh_len = 0;

    // cache
    int cache_size;
    int num_strings_cache = 0;

    // objects
    char objects[MAX_OBJ][NAME_BUFF_SIZE];
    int objects_len = 0;

    // interest list
    Interest interests[MAX_INTEREST];
    int interests_len = 0;

    int abvr = 0; // abreviation of commands

    // TCP
    int listening_fd = 0, newsock_fd;
    char out_tcp_buffer[TCP_BUFF_SIZE];

    // UDP
    int registered = 0;
    char net[4];
    int udpsock_fd = 0;     //descritor
    char udp_buffer[UDP_BUFF_SIZE];
    struct addrinfo *res_udp = NULL;
    Node_Addr server_addr;

    // STDIN
    char stdin_buffer[STDIN_BUFF_SIZE];

    // select
    int max_fd = 0;
    fd_set set_fd;
    int select_cntr;

    // rand
    srand(time(NULL));

    // 4 or 6 arguments
    if (argc != 4 && argc != 6) {
        error(  "ERROR: Número de argumentos inválido"
                "( ./ndn <cache> <IP> <TCP> |./ndn <cache> <IP> <TCP> <regIP> <regUDP> )"
             );
    }

    // check if cache is a number
    if ((cache_size = atoi(argv[1])) == 0) {
        error("ERROR: Argumento inválido <cache> é nulo ou inválido");
    }
    // initialize cache
    char cache[cache_size][NAME_BUFF_SIZE];

    // check if IP is a valid IPv4 address
    if (inet_pton(AF_INET, argv[2], own_addr.ip) != 1) {
        error("ERROR: Argumento inválido <IP> não é um endereço IPv4 válido");
    }
    snprintf(own_addr.ip, IP_LEN, "%s", argv[2]);

    // check if TCP is a valid port number
    if (atoi(argv[3]) > 65535 || atoi(argv[3]) < 1024) {
        error("ERROR: Argumento inválido <TCP> não é um número de porta válido");
    }
    snprintf(own_addr.port, PORT_LEN, "%s", argv[3]);

    if (argc == 6) {
        // check if IP is a valid IPv4 address
        if (inet_pton(AF_INET, argv[4], server_addr.ip) != 1) {
            error("ERROR: Argumento inválido <regIP> não é um endereço IPv4 válido");
        }
        snprintf(server_addr.ip, IP_LEN, "%s", argv[4]);

        // check if TCP is a valid port number
        if (atoi(argv[5]) > 65535 || atoi(argv[5]) < 1024) {
            error("ERROR: Argumento inválido <regUDP> não é um número de porta válido");
        }
        snprintf(server_addr.port, PORT_LEN, "%s", argv[5]);

    }

    else {
        // default
        snprintf(server_addr.ip, IP_LEN, "%s", UDP_SV_IP);
        snprintf(server_addr.port, PORT_LEN, "%s", UDP_SV_PORT);

    }

    print_help();

    while (1) {

        // reset descriptor set
        FD_ZERO(&set_fd);
        // set stdin in descriptor set
        FD_SET(STDIN, &set_fd);

        // check max descriptor
        max_fd = max(max_fd, STDIN);

        if (listening_fd != 0) {
            // set listening_fd in descriptor set
            FD_SET(listening_fd, &set_fd);
            // repeat...
            max_fd = max(max_fd, listening_fd);
        }

        for (int i = 0; i < neigh_len; i++) {
            // set intr_fd[i] in descriptor set
            FD_SET(neigh_addr_fd[i].fd, &set_fd);
            max_fd = max(max_fd, neigh_addr_fd[i].fd);
        }

        select_cntr = select(max_fd + 1, &set_fd, NULL, NULL, NULL);

        // STDIN
        if (FD_ISSET(STDIN, &set_fd)) {

            // decrement select counter
            select_cntr--;
            // read stdin
            //memset(stdin_buffer, 0, STDIN_BUFF_SIZE); // acho que dá para tirar isto
            fgets(stdin_buffer, STDIN_BUFF_SIZE, stdin);

            if ((abvr = 0, strncmp(stdin_buffer, "join ", 5) == 0) || (abvr = 1, strncmp(stdin_buffer, "j ", 2) == 0)) {
                // join
                int len;
                char connectIP[IP_LEN];
                char connectTCP[PORT_LEN];

                if (abvr) {   //tirar o valor da net
                    // abreviation
                    sscanf(stdin_buffer, "j %3s", net);
                } else {
                    // no abreviation
                    sscanf(stdin_buffer, "join %3s", net);
                }

                // UDP socket
                udpsock_fd = create_udp_client_socket();
                // UDP server info
                res_udp = get_udp_server_info(server_addr.ip, server_addr.port);
                // get NODES on net
                len = snprintf(udp_buffer, UDP_BUFF_SIZE, "NODES %s", net);    // mandar as net para o buffer

                udp_send(udpsock_fd, res_udp, udp_buffer, len);
                udp_receive(udpsock_fd, udp_buffer, UDP_BUFF_SIZE);
                if (strncmp(udp_buffer, "NODESLIST ", 10) != 0)
                    error("ERROR: pedido de lista de nós na rede do servidor falhou");

                // TCP listening socket;
                listening_fd = create_tcp_server_socket(own_addr.port);
                created = 1;

                // next '\n'
                char *tok_udp = strchr(udp_buffer, '\n');
                // last '\n'
                char *term_udp = strrchr(udp_buffer, '\n');

                if (tok_udp == term_udp) {
                    // no nodes in server
                    // set safe as own
                    cpy_Node_Addr(safe_addr, own_addr);
                    // set ext as own (by default)
                    cpy_Node_Addr(ext_addr, own_addr);
                }
                
                else {
                    // options to choose from
                    char *opt[MAX_NODES];
                    char *aux = strchr(tok_udp + 1, '\n');
                    int i = 0;
                    while (tok_udp != term_udp && i < MAX_NODES) {

                        opt[i] = tok_udp + 1;
                        i++;
                        tok_udp = aux;
                        *aux = '\0';
                        aux = strchr(aux + 1, '\n');

                    }

                    // make less predictable
                    int the_chosen_one = rand() % i;
                    // get connect IP and TCP
                    sscanf(opt[the_chosen_one], "%s %s", connectIP, connectTCP);

                    // connect to external neighbor
                    // external neighbor TCP socket
                    neigh_addr_fd[neigh_len].fd = create_tcp_client_socket(connectIP, connectTCP);
                    // set connecting node as external
                    snprintf(ext_addr.ip, IP_LEN, "%s", connectIP);
                    snprintf(ext_addr.port, PORT_LEN, "%s", connectTCP);
                    // set connecting node as a neighbor
                    snprintf(neigh_addr_fd[neigh_len].node_addr.ip, IP_LEN, "%s", connectIP);
                    snprintf(neigh_addr_fd[neigh_len].node_addr.port, PORT_LEN, "%s", connectTCP);

                    // send ENTRY
                    len = snprintf(out_tcp_buffer, TCP_BUFF_SIZE, "ENTRY %s %s\n", own_addr.ip, own_addr.port);
                    tcp_send(neigh_addr_fd[neigh_len].fd, out_tcp_buffer, len);

                    neigh_len++;

                }

                // register node
                len = snprintf(udp_buffer, UDP_BUFF_SIZE, "REG %s %s %s", net, own_addr.ip, own_addr.port); 
                udp_send(udpsock_fd, res_udp, udp_buffer, len);
                // confirm registration
                udp_receive(udpsock_fd, udp_buffer, UDP_BUFF_SIZE);
                if (strncmp(udp_buffer, "OKREG", 5) != 0)
                    error("ERROR: registo de nó no servidor de nós falhou");

                registered = 1;

            } else if ((abvr = 0, strncmp(stdin_buffer, "direct join ", 12) == 0) || (abvr = 1, strncmp(stdin_buffer, "dj ", 3) == 0)) {
                // direct join
                char connectIP[IP_LEN];
                char connectTCP[PORT_LEN];
                int len = 0;

                if (abvr) {
                    // abreviation
                    sscanf(stdin_buffer, "dj %15s %5s", connectIP, connectTCP);
                } else {
                    // no abreviation
                    sscanf(stdin_buffer, "direct join %15s %5s", connectIP, connectTCP);
                }

                char aux[IP_LEN];
                // check if IP is a valid IPv4 address
                if (inet_pton(AF_INET, connectIP, aux) != 1) {
                    error("ERROR: Argumento inválido <IP> não é um endereço IPv4 válido");
                }

                // listening TCP socket
                listening_fd = create_tcp_server_socket(own_addr.port);

                if (strcmp(connectIP, "0.0.0.0") == 0) {
                    // create network with only this node
                    // set safe as own
                    cpy_Node_Addr(safe_addr, own_addr);
                    // set ext as own (by default)
                    cpy_Node_Addr(ext_addr, own_addr);

                } else {
                    // connect to node
                    if (atoi(connectTCP) > 65535 || atoi(connectTCP) < 1024) {
                        fprintf(stderr, "ERROR: Argumento inválido <connectTCP> não é um número de porta válido");
                        continue;
                    };

                    // external neighbor TCP socket
                    neigh_addr_fd[neigh_len].fd = create_tcp_client_socket(connectIP, connectTCP);

                    // set external neighbor
                    snprintf(ext_addr.ip, IP_LEN, "%s", connectIP);
                    snprintf(ext_addr.port, PORT_LEN, "%s", connectTCP);
                    // set external neighbor
                    snprintf(neigh_addr_fd[neigh_len].node_addr.ip, IP_LEN, "%s", connectIP);
                    snprintf(neigh_addr_fd[neigh_len].node_addr.port, PORT_LEN, "%s", connectTCP);

                    // send ENTRY
                    len = snprintf(out_tcp_buffer, TCP_BUFF_SIZE, "ENTRY %s %s\n", own_addr.ip, own_addr.port);
                    tcp_send(neigh_addr_fd[neigh_len].fd, out_tcp_buffer, len);

                    neigh_len++;
                    
                }

                created = 1;
                
            } else if ((abvr = 0, strncmp(stdin_buffer, "create ", 7) == 0) || (abvr = 1, strncmp(stdin_buffer, "c ", 2) == 0)) {
                // create

                char name [NAME_BUFF_SIZE];

                // Extrai o nome do objeto do input do utilizador
                if (abvr) {
                    // abreviation
                    sscanf(stdin_buffer, "c %100s", name);
                } else {
                    // no abreviation
                    sscanf(stdin_buffer, "create %100s", name);
                }

                if (created == 0) {
                    printf("ERROR: nó não criado\n");
                }
                else if(objects_len >= cache_size){
                    printf("ERROR: cache cheia\n");
                }
                else {

                    snprintf(objects[objects_len], NAME_BUFF_SIZE, "%s", name);
                    objects_len++;

                }

            } else if ((abvr = 0, strncmp(stdin_buffer, "delete ", 7) == 0) || (abvr = 1, strncmp(stdin_buffer, "dl ", 3) == 0)) {
                // delete

                char name [NAME_BUFF_SIZE];
                int i;

                // Extrai o nome do objeto do input do utilizador
                if (abvr) {
                    // abreviation
                    sscanf(stdin_buffer, "dl %100s", name);
                } else {
                    // no abreviation
                    sscanf(stdin_buffer, "delete %100s", name);
                }

                if (created == 0) {
                    printf("ERROR: nó não criado\n");
                }
                else if(objects_len <= 0){
                    printf("ERROR: objetos vazios\n");
                }
                else {

                    for (i = 0; i < objects_len; i++) {
                        if (strncmp(objects[i], name, NAME_BUFF_SIZE) == 0) {
                            break;
                        }
                    }

                    if (i >= objects_len) {
                        printf("ERROR: <name> não encontrado\n");
                    }
                    else {
                        // delete
                        for (; i < objects_len - 1; i++) {
                            strncpy(objects[i],  objects[i + 1], NAME_BUFF_SIZE);
                        }
                        objects_len--;

                    }

                }

            } else if (abvr = 0, (strncmp(stdin_buffer, "retrieve ", 9) == 0) || (abvr = 1, strncmp(stdin_buffer, "r ", 2) == 0)) {
                // retrieve

                char name [NAME_BUFF_SIZE];
                int i_cache, i_objects;

                // Extrai o nome do objeto do input do utilizador
                if (abvr) {
                    // abreviation
                    sscanf(stdin_buffer, "r %100s", name);
                } else {
                    // no abreviation
                    sscanf(stdin_buffer, "retrieve %100s", name);
                }

                for (i_cache = 0; i_cache < num_strings_cache; i_cache++) {
                    if (strncmp(cache[i_cache], name, NAME_BUFF_SIZE) == 0) {
                        break;
                    }
                }

                for (i_objects = 0; i_objects < objects_len; i_objects++) {
                    if (strncmp(objects[i_objects], name, NAME_BUFF_SIZE) == 0) {
                        break;
                    }
                }

                if (i_cache >= num_strings_cache && i_objects >= objects_len) {

                    if (neigh_len == 0) {
                        printf("ERROR: <name> não encontrado e sem vizinhos\n");
                    }
                    else {

                        snprintf(interests[interests_len].name, NAME_BUFF_SIZE, "%s", name);

                        int len = snprintf(out_tcp_buffer, TCP_BUFF_SIZE, "INTEREST %s\n", name);
                        for (int j = 0; j < neigh_len; j++) {
                            tcp_send(neigh_addr_fd[j].fd, out_tcp_buffer, len);
                            interests[interests_len].state_fd[j].fd = neigh_addr_fd[j].fd;
                            interests[interests_len].state_fd[j].state = WAIT;
                        }

                        interests[interests_len].state_fd_len = neigh_len;
                        interests_len++;

                        printf("INTEREST:\n");
                        for (int i = 0; i < interests_len; i++) {
                            printf("%s ", interests[i].name);
                            for (int j = 0; j < interests[i].state_fd_len; j++) {
                                printf("%d:", interests[i].state_fd[j].fd);
                                switch (interests[i].state_fd[j].state)
                                {
                                case WAIT:
                                    printf("ESPERA ");
                                    break;
                                
                                case CLOSE:
                                printf("FECHADO ");
                                break;

                                case ANSWER:
                                printf("RESPOSTA ");
                                break;
                                
                                default:
                                    break;
                                }
                            }
                            printf("\n");
                        }
                    }
                }

            } else if (strncmp(stdin_buffer, "show topology\n", 14) == 0 || strncmp(stdin_buffer, "st\n", 3) == 0) {
                // show topology
                if (created == 0)
                    fprintf(stderr, "ERROR: Nó não criado\n");
                else {
                    // external
                    printf("External: ");
                    print_node_addr(ext_addr);
                    printf("\n");
                    // safe
                    printf("Safe: ");
                    print_node_addr(safe_addr);
                    printf("\n");
                    // internal
                    printf("Internal: ");

                    int printed = 0;
                    for (int i = 0; i < neigh_len; i++) {
                        if ((!cmp_Node_Addr(ext_addr, neigh_addr_fd[i].node_addr)) || cmp_Node_Addr(safe_addr, own_addr)) {
                            print_node_addr(neigh_addr_fd[i].node_addr);
                            printf(" ");
                            printed = 1;
                        }
                    }
                    if (!printed)
                        printf("NULL");
                    printf("\n");
                }

            } else if (strncmp(stdin_buffer, "show names\n", 11) == 0 || strncmp(stdin_buffer, "sn\n", 3) == 0) {
                // show names

                if (created == 0) {
                    printf("ERROR: nó não criado\n");
                }
                else {
                    if (num_strings_cache == 0) {
                        printf("NAMES (CACHE): NULL\n");
                    }
                    else {
                        printf("NAMES (CACHE):\n");
                        for (int i = 0; i < num_strings_cache; i++) {
                            printf("\t%s\n", cache[i]);
                        }
                    }

                    printf("\n");

                    if (objects_len == 0) {
                        printf("NAMES (OBJECTS): NULL\n");
                    }
                    else {
                        printf("NAMES (OBJECTS):\n");
                        for (int i = 0; i < objects_len; i++) {
                            printf("\t%s\n", objects[i]);
                        }
                    }
                }

            } else if (strncmp(stdin_buffer, "show interest table\n", 20) == 0 || strncmp(stdin_buffer, "si\n", 3) == 0) {
                // show interest table

                if (created == 0) {
                    printf("ERROR: nó não criado\n");
                }
                else if (interests_len == 0) {
                    printf("INTEREST: NULL\n");
                }
                else {
                    printf("INTEREST:\n");
                    for (int i = 0; i < interests_len; i++) {
                        printf("%s ", interests[i].name);
                        for (int j = 0; j < interests[i].state_fd_len; j++) {
                            printf("%d:", interests[i].state_fd[j].fd);
                            switch (interests[i].state_fd[j].state)
                            {
                            case WAIT:
                                printf("ESPERA ");
                                break;
                            
                            case CLOSE:
                            printf("FECHADO ");
                            break;

                            case ANSWER:
                            printf("RESPOSTA ");
                            break;
                            
                            default:
                                break;
                            }
                        }
                        printf("\n");
                    }

                }

            } else if (strncmp(stdin_buffer, "leave\n", 6) == 0 || strncmp(stdin_buffer, "l\n", 2) == 0) {
                // leave
                int len;

                if (created == 0) {
                    printf("ERROR: nó não criado\n");
                }
                else {
                    if (registered != 0) {
                        // unregister node
                        len = snprintf(udp_buffer, UDP_BUFF_SIZE, "UNREG %s %s %s", net, own_addr.ip, own_addr.port);
                        udp_send(udpsock_fd, res_udp, udp_buffer, len);
                        // confirm unregistration
                        udp_receive(udpsock_fd, udp_buffer, UDP_BUFF_SIZE);
                        if (strncmp(udp_buffer, "OKUNREG", 7) != 0)
                            error("ERROR: cancelamento de registo de nó no servidor de nós falhou");
                    }

                    if (res_udp != 0) {
                        // was allocated
                        freeaddrinfo(res_udp);
                        res_udp = 0;
                    }

                    // close descriptors
                    if (listening_fd != 0) {
                        // was open
                        close(listening_fd);
                        listening_fd = 0;
                    }

                    if (udpsock_fd != 0) {
                        // was open
                        close(udpsock_fd);
                        udpsock_fd = 0;
                    }

                    for (int i = 0; i < neigh_len; i++) {
                        close(neigh_addr_fd[i].fd);
                        neigh_addr_fd[i].fd = 0;
                    }

                    // reset variables
                    memset(&ext_addr, 0, sizeof(Node_Addr));
                    memset(&safe_addr, 0, sizeof(Node_Addr));
                    for (int i = 0; i < neigh_len; i++) {
                        memset(&neigh_addr_fd[i], 0, sizeof(Node_Addr_FD));
                    }
                    created = 0;
                    neigh_len = 0;
                    num_strings_cache = 0;
                    interests_len = 0;
                    objects_len = 0;
                }

            } else if (strncmp(stdin_buffer, "exit\n", 5) == 0 || strncmp(stdin_buffer, "x\n", 2) == 0) {
                // exit
                if (created == 1) {
                    printf("ERROR: Nó criado. Experimente o comando leave antes.\n");  
                }
                else
                    exit(0);

            } else {
                fprintf(stderr, "ERROR: Comando inválido\n");
                print_help();
            }

            if (select_cntr == 0) {
                continue;
            }

        }

        // TCP IN
        if (FD_ISSET(listening_fd, &set_fd)) {

            // decrement select counter
            select_cntr--;
            // accept new TCP connection and get new socket descriptor
            newsock_fd = accept(listening_fd, NULL, NULL);
            if (newsock_fd == -1) {
                error("ERROR: accept falhou");
            }

            if (neigh_len >= MAX_NEIGH) {
                // full of internal neighbors
                error("ERROR: Número máximo de nós internos atingido");
            }
            // save socket descriptor
            neigh_addr_fd[neigh_len].fd = newsock_fd;
            neigh_len++;

            if (select_cntr == 0) {
                continue;
            }

        }

        // CHECK NEIGHBORS
        for (int i = 0; i < neigh_len; i++) {
            if (FD_ISSET(neigh_addr_fd[i].fd, &set_fd)) {
                // descriptor UP for reading
                // is it the external node
                int is_ext = cmp_Node_Addr(ext_addr, neigh_addr_fd[i].node_addr);
                // decrement select counter
                select_cntr--;

                int next_newline = 0;
                int len = 0;
                int seperated = 0;
                // read from descriptor
                int n = tcp_receive(neigh_addr_fd[i].fd, neigh_addr_fd[i].buffer + neigh_addr_fd[i].buffer_len, NEIGH_BUFF_SIZE - neigh_addr_fd[i].buffer_len);
                printf("%s", neigh_addr_fd[i].buffer);

                if (n == 0) {
                    // connection closed on other side
                    close(neigh_addr_fd[i].fd);

                    if (!is_ext) {
                        for (int j = i; j < neigh_len - 1; j++) {
                            neigh_addr_fd[j] = neigh_addr_fd[j + 1];
                        }
                        neigh_len--;
                    }

                    else {
                        if (!cmp_Node_Addr(safe_addr, own_addr)) {

                            neigh_addr_fd[i].fd = create_tcp_client_socket(safe_addr.ip, safe_addr.port);
                            cpy_Node_Addr(neigh_addr_fd[i].node_addr, safe_addr);
                            cpy_Node_Addr(ext_addr, safe_addr);

                            // send ENTRY
                            len = snprintf(out_tcp_buffer, TCP_BUFF_SIZE, "ENTRY %s %s\n", own_addr.ip, own_addr.port);
                            tcp_send(neigh_addr_fd[i].fd, out_tcp_buffer, len);

                            for (int j = 0; j < neigh_len; j++) {
                                if (j != i) {
                                    // send SAFE
                                    len = snprintf(out_tcp_buffer, TCP_BUFF_SIZE, "SAFE %s %s\n", ext_addr.ip, ext_addr.port);
                                    tcp_send(neigh_addr_fd[j].fd, out_tcp_buffer, len);
                                }
                            }

                        }

                        else {

                            for (int j = i; j < neigh_len - 1; j++) {
                                neigh_addr_fd[j] = neigh_addr_fd[j + 1];
                            }
                            neigh_len--;

                            if (neigh_len > 0) {
                                
                                int the_chosen_one = rand() % neigh_len;

                                cpy_Node_Addr(ext_addr, neigh_addr_fd[the_chosen_one].node_addr);

                                // send ENTRY
                                len = snprintf(out_tcp_buffer, TCP_BUFF_SIZE, "ENTRY %s %s\n", own_addr.ip, own_addr.port);
                                tcp_send(neigh_addr_fd[the_chosen_one].fd, out_tcp_buffer, len);

                                for (int j = 0; j < neigh_len; j++) {
                                    // send SAFE
                                    len = snprintf(out_tcp_buffer, TCP_BUFF_SIZE, "SAFE %s %s\n", ext_addr.ip, ext_addr.port);
                                    tcp_send(neigh_addr_fd[j].fd, out_tcp_buffer, len);
                                }

                            }

                            else {

                                cpy_Node_Addr(ext_addr, own_addr);

                            }
                        }
                    }

                }

                else {

                    char *check_newline = strrchr(neigh_addr_fd[i].buffer, '\n');
                    if (check_newline == NULL) {
                        // no '\n' in buffer
                        neigh_addr_fd[i].buffer_len += n;
                        if (neigh_addr_fd[i].buffer_len >= NEIGH_BUFF_SIZE) {
                            // buffer full
                            // reset offset, as there is no way the buffer does not have enough space for a command
                            neigh_addr_fd[i].buffer_len = 0;
                        }
                        continue;
                    }

                    int last_newline = strrchr(neigh_addr_fd[i].buffer, '\n') - neigh_addr_fd[i].buffer + 1;

                    if (last_newline - neigh_addr_fd[i].buffer_len != n) {
                        // a command was seperated in two buffers
                        seperated = 1;
                    }
                    else {
                        seperated = 0;
                        neigh_addr_fd[i].buffer_len = 0;
                    }

                    while (next_newline < last_newline) {
                        if (strncmp(neigh_addr_fd[i].buffer + next_newline, "ENTRY", 5) == 0) {
                            // ENTRY
                            // get internal address
                            sscanf(neigh_addr_fd[i].buffer + next_newline, "ENTRY %s %s\n", neigh_addr_fd[i].node_addr.ip, neigh_addr_fd[i].node_addr.port);

                            if (cmp_Node_Addr(ext_addr, own_addr)) {
                                // don't have an external neighbor
                                // (ext is own by default)
                                cpy_Node_Addr(ext_addr, neigh_addr_fd[i].node_addr);
                                // send SAFE (1st answer)
                                len = snprintf(out_tcp_buffer, TCP_BUFF_SIZE, "SAFE %s %s\n", ext_addr.ip, ext_addr.port);
                                // send ENTRY (2nd send my own command)
                                len += snprintf(out_tcp_buffer + len, TCP_BUFF_SIZE, "ENTRY %s %s\n", own_addr.ip, own_addr.port);
                                tcp_send(neigh_addr_fd[i].fd, out_tcp_buffer, len);

                            }

                            else {
                                // ext is not own
                                // send SAFE
                                len = snprintf(out_tcp_buffer, TCP_BUFF_SIZE, "SAFE %s %s\n", ext_addr.ip, ext_addr.port);
                                tcp_send(neigh_addr_fd[i].fd, out_tcp_buffer, len);

                            }
                        }

                        else if (strncmp(neigh_addr_fd[i].buffer + next_newline, "SAFE", 4) == 0) {
                            // SAFE
                            // get safeguard address
                            sscanf(neigh_addr_fd[i].buffer + next_newline, "SAFE %s %s\n", safe_addr.ip, safe_addr.port);
        
                        } 

                        else if (strncmp(neigh_addr_fd[i].buffer + next_newline, "INTEREST", 8) == 0) {
                            // INTEREST
                            char name [NAME_BUFF_SIZE];
                            int j;
                            int k;
                            // get name
                            sscanf(neigh_addr_fd[i].buffer + next_newline, "INTEREST %s\n", name);

                            for (j = 0; j < num_strings_cache; j++) {
                                if (strncmp(cache[j], name, NAME_BUFF_SIZE) == 0) {
                                    break;
                                }
                            }

                            if (j < num_strings_cache) {
                                // found
                                len = snprintf(out_tcp_buffer, TCP_BUFF_SIZE, "OBJECT %s\n", cache[j]);
                                tcp_send(neigh_addr_fd[i].fd, out_tcp_buffer, len);
                                // print interest table
                                if (interests_len == 0) {
                                    printf("INTEREST:\n");
                                }
                                else {
                                    printf("INTEREST:\n");
                                    for (int p = 0; p < interests_len; p++) {
                                        printf("%s ", interests[p].name);
                                        for (int q = 0; q < interests[p].state_fd_len; q++) {
                                            printf("%d:", interests[p].state_fd[q].fd);
                                            switch (interests[p].state_fd[q].state)
                                            {
                                            case WAIT:
                                                printf("ESPERA ");
                                                break;
                                            
                                            case CLOSE:
                                            printf("FECHADO ");
                                            break;

                                            case ANSWER:
                                            printf("RESPOSTA ");
                                            break;
                                            
                                            default:
                                                break;
                                            }
                                        }
                                        printf("\n");
                                    }
                                }
                                printf("%s %d:FECHADO\n", name, neigh_addr_fd[i].fd);
                            }
                            else {

                                for (j = 0; j < objects_len; j++) {
                                    if (strncmp(objects[j], name, NAME_BUFF_SIZE) == 0) {
                                        break;
                                    }
                                }

                                if (j < objects_len) {
                                    // found
                                    len = snprintf(out_tcp_buffer, TCP_BUFF_SIZE, "OBJECT %s\n", objects[j]);
                                    tcp_send(neigh_addr_fd[i].fd, out_tcp_buffer, len);
                                    // print interest table
                                    if (interests_len == 0) {
                                        printf("INTEREST:\n");
                                    }
                                    else {
                                        printf("INTEREST:\n");
                                        for (int p = 0; p < interests_len; p++) {
                                            printf("%s ", interests[p].name);
                                            for (int q = 0; q < interests[p].state_fd_len; q++) {
                                                printf("%d:", interests[p].state_fd[q].fd);
                                                switch (interests[p].state_fd[q].state)
                                                {
                                                case WAIT:
                                                    printf("ESPERA ");
                                                    break;
                                                
                                                case CLOSE:
                                                printf("FECHADO ");
                                                break;

                                                case ANSWER:
                                                printf("RESPOSTA ");
                                                break;
                                                
                                                default:
                                                    break;
                                                }
                                            }
                                            printf("\n");
                                        }
                                    }
                                    printf("%s %d:FECHADO\n", name, neigh_addr_fd[i].fd);
                                    
                                }

                                else {
                                    // not found

                                    if (neigh_len == 1) {
                                        len = snprintf(out_tcp_buffer, TCP_BUFF_SIZE, "NOOBJECT %s\n", name);
                                        tcp_send(neigh_addr_fd[i].fd, out_tcp_buffer, len);
                                        // print interest table
                                        if (interests_len == 0) {
                                            printf("INTEREST:\n");
                                        }
                                        else {
                                            printf("INTEREST:\n");
                                            for (int p = 0; p < interests_len; p++) {
                                                printf("%s ", interests[p].name);
                                                for (int q = 0; q < interests[p].state_fd_len; q++) {
                                                    printf("%d:", interests[p].state_fd[q].fd);
                                                    switch (interests[p].state_fd[q].state)
                                                    {
                                                    case WAIT:
                                                        printf("ESPERA ");
                                                        break;
                                                    
                                                    case CLOSE:
                                                    printf("FECHADO ");
                                                    break;
        
                                                    case ANSWER:
                                                    printf("RESPOSTA ");
                                                    break;
                                                    
                                                    default:
                                                        break;
                                                    }
                                                }
                                                printf("\n");
                                            }
                                        }
                                        printf("%s %d:FECHADO\n", name, neigh_addr_fd[i].fd);
                                    
                                    }

                                    else {

                                        for (j = 0; j < interests_len; j++) {
                                            if (strncmp(interests[j].name, name, NAME_BUFF_SIZE) == 0) {
                                                break;
                                            }
                                        }
                                    
                                        if (j >= interests_len) {
                                            // interest not found
                                            // create interest table entry
                                            if (interests_len == MAX_INTEREST) {
                                                error("ERROR: número máximo de interesses atingido");
                                            }

                                            // create new interest list entry
                                            snprintf(interests[interests_len].name, NAME_BUFF_SIZE, "%s", name);
                                            interests[interests_len].state_fd_len = 0;
                                            // prepare to send INTEREST to all but current neighbor
                                            int next_newline = snprintf(out_tcp_buffer, TCP_BUFF_SIZE, "INTEREST %s\n", name);
                                            for (j = 0; j < neigh_len; j++) {
                                                if (i != j) {
                                                    tcp_send(neigh_addr_fd[j].fd, out_tcp_buffer, next_newline);
                                                    interests[interests_len].state_fd[interests[interests_len].state_fd_len].fd = neigh_addr_fd[j].fd;
                                                    interests[interests_len].state_fd[interests[interests_len].state_fd_len].state = WAIT;
                                                    interests[interests_len].state_fd_len++;
                                                }
                                            }

                                            interests[interests_len].state_fd[interests[interests_len].state_fd_len].fd = neigh_addr_fd[i].fd;
                                            interests[interests_len].state_fd[interests[interests_len].state_fd_len].state = ANSWER;
                                            interests[interests_len].state_fd_len++;
                    
                                            interests_len++;

                                            // print interest table
                                            if (interests_len == 0) {
                                                printf("INTEREST: NULL\n");
                                            }
                                            else {
                                                printf("INTEREST:\n");
                                                for (int p = 0; p < interests_len; p++) {
                                                    printf("%s ", interests[p].name);
                                                    for (int q = 0; q < interests[p].state_fd_len; q++) {
                                                        printf("%d:", interests[p].state_fd[q].fd);
                                                        switch (interests[p].state_fd[q].state)
                                                        {
                                                        case WAIT:
                                                            printf("ESPERA ");
                                                            break;
                                                        
                                                        case CLOSE:
                                                        printf("FECHADO ");
                                                        break;
            
                                                        case ANSWER:
                                                        printf("RESPOSTA ");
                                                        break;
                                                        
                                                        default:
                                                            break;
                                                        }
                                                    }
                                                    printf("\n");
                                                }
                                            }

                                        }

                                        else {
                                            // found interest table entry

                                            for (k = 0; k < interests[j].state_fd_len; k++) {
                                                if (neigh_addr_fd[i].fd == interests[j].state_fd[k].fd) {
                                                    break;
                                                }
                                            }

                                            if (k == interests[j].state_fd_len) {
                                                // not found descriptor in entry

                                                if (k == MAX_NEIGH) {
                                                    error("ERROR: número máximo de interfaces de interesse atingidas");
                                                }
                                                // add new interface to interest
                                                interests[j].state_fd[k].fd = neigh_addr_fd[i].fd;
                                                interests[j].state_fd_len++;
                                            }

                                            interests[j].state_fd[k].state = ANSWER;

                                            // print

                                            // am i still waiting?
                                            int wait = 0;
                                            for (k = 0; k < interests[j].state_fd_len; k++) {
                                                if (interests[j].state_fd[k].state == WAIT)
                                                    wait = 1;
                                            }
                                            // if not, send NOOBJECT to all in ANSWER state
                                            if (wait == 0) {
                                                len = snprintf(out_tcp_buffer, TCP_BUFF_SIZE, "NOOBJECT %s\n", name);
                                                for (k = 0; k < interests[j].state_fd_len; k++) {
                                                    if (interests[j].state_fd[k].state == ANSWER)
                                                        tcp_send(interests[j].state_fd[k].fd, out_tcp_buffer, len);
                                                }
                                                
                                                // remove interest
                                                for (; j < interests_len; j++) {
                                                    interests[j] = interests[j + 1];
                                                }

                                                interests_len--;

                                                // print interest table
                                                if (interests_len == 0) {
                                                    printf("INTEREST: NULL\n");
                                                }
                                                else {
                                                    printf("INTEREST:\n");
                                                    for (int p = 0; p < interests_len; p++) {
                                                        printf("%s ", interests[p].name);
                                                        for (int q = 0; q < interests[p].state_fd_len; q++) {
                                                            printf("%d:", interests[p].state_fd[q].fd);
                                                            switch (interests[p].state_fd[q].state)
                                                            {
                                                            case WAIT:
                                                                printf("ESPERA ");
                                                                break;
                                                            
                                                            case CLOSE:
                                                            printf("FECHADO ");
                                                            break;
                
                                                            case ANSWER:
                                                            printf("RESPOSTA ");
                                                            break;
                                                            
                                                            default:
                                                                break;
                                                            }
                                                        }
                                                        printf("\n");
                                                    }
                                                }

                                            }



                                        }

                                    }

                                }

                            }

                        }

                        else if (strncmp(neigh_addr_fd[i].buffer + next_newline, "OBJECT", 6) == 0) {
                            // OBJECT
                            char name[NAME_BUFF_SIZE];
                            int j;
                            int k;

                            sscanf(neigh_addr_fd[i].buffer, "OBJECT %s\n", name);

                            // search which interest has correspondence to name
                            for (j = 0; j < interests_len; j++) {
                                if (strncmp(interests[j].name, name, NAME_BUFF_SIZE) == 0) {
                                    break;
                                }
                            }

                            if (j < interests_len) {
                                // found interest table entry
                                // search where the descriptor is in the interest
                                for (k = 0; k < interests[j].state_fd_len; k++) {

                                    if (interests[j].state_fd[k].fd == neigh_addr_fd[i].fd) {
                                        break;
                                    }

                                }

                                if (k == interests[j].state_fd_len) {
                                    // not found descriptor in entry
                                    printf("ERROR: descritor que enviou OBJECT não está no INTEREST correspondente");
                                }
                                
                                snprintf(cache[num_strings_cache], NAME_BUFF_SIZE, "%s", name);
                                num_strings_cache++;

                                // send to all waiting ANSWER
                                len = snprintf(out_tcp_buffer, TCP_BUFF_SIZE, "OBJECT %s\n", name);
                                for (k = 0; k < interests[j].state_fd_len; k++) {
                                    if (interests[j].state_fd[k].state == ANSWER)
                                        tcp_send(interests[j].state_fd[k].fd, out_tcp_buffer, len);
                                }

                                // remove interest
                                for (; j < interests_len; j++) {
                                    interests[j] = interests[j + 1];
                                }

                                interests_len--;

                                if (interests_len == 0) {
                                    printf("INTEREST: NULL\n");
                                }
                                else {
                                    printf("INTEREST:\n");
                                    for (int p = 0; p < interests_len; p++) {
                                        printf("%s ", interests[p].name);
                                        for (int q = 0; q < interests[p].state_fd_len; q++) {
                                            printf("%d:", interests[p].state_fd[q].fd);
                                            switch (interests[p].state_fd[q].state)
                                            {
                                            case WAIT:
                                                printf("ESPERA ");
                                                break;
                                            
                                            case CLOSE:
                                            printf("FECHADO ");
                                            break;

                                            case ANSWER:
                                            printf("RESPOSTA ");
                                            break;
                                            
                                            default:
                                                break;
                                            }
                                        }
                                        printf("\n");
                                    }
                                }

                            }

                        }

                        else if (strncmp(neigh_addr_fd[i].buffer + next_newline, "NOOBJECT", 8) == 0) {
                            // NOOBJECT
                            char name[NAME_BUFF_SIZE];
                            int j = 0;
                            int k = 0;

                            sscanf(neigh_addr_fd[i].buffer, "NOOBJECT %s\n", name);

                            for (j = 0; j < interests_len; j++) {
                                if (strncmp(interests[j].name, name, NAME_BUFF_SIZE) == 0) {
                                    break;
                                }
                            }

                            if (j < interests_len) {
                                // found interest table entry
                                for (k = 0; k < interests[j].state_fd_len; k++) {
                                    if (interests[j].state_fd[k].fd == neigh_addr_fd[i].fd) {
                                        break;
                                    }
                                }
                                if (k == interests[j].state_fd_len) {
                                    printf("ERROR: NOOBJECT recebido por descritor não associado");
                                    continue;
                                }

                                interests[j].state_fd[k].state = CLOSE;

                                // am i still waiting for any descriptor?
                                int wait = 0;
                                for (k = 0; k < interests[j].state_fd_len; k++) {
                                    if (interests[j].state_fd[k].state == WAIT)
                                        wait = 1;
                                }

                                if (wait == 0) {
                                    // if not
                                    // send NOOBJECT to all in ANSWER state
                                    len = snprintf(out_tcp_buffer, TCP_BUFF_SIZE, "NOOBJECT %s\n", name);
                                    for (k = 0; k < interests[j].state_fd_len; k++) {
                                        if (interests[j].state_fd[k].state == ANSWER) {
                                            tcp_send(interests[j].state_fd[k].fd, out_tcp_buffer, len);
                                        }
                                    }

                                    // delete interest table entry
                                    for (; j < interests_len; j++) {
                                        interests[j] = interests[j + 1];
                                    }
                                    interests_len--;

                                    printf("ERROR: não foi encontrado %s na rede.\n", name);

                                }

                                if (interests_len == 0) {
                                    printf("INTEREST: NULL\n");
                                }
                                else {
                                    printf("INTEREST:\n");
                                    for (int p = 0; p < interests_len; p++) {
                                        printf("%s ", interests[p].name);
                                        for (int q = 0; q < interests[p].state_fd_len; q++) {
                                            printf("%d:", interests[p].state_fd[q].fd);
                                            switch (interests[p].state_fd[q].state)
                                            {
                                            case WAIT:
                                                printf("ESPERA ");
                                                break;
                                            
                                            case CLOSE:
                                            printf("FECHADO ");
                                            break;

                                            case ANSWER:
                                            printf("RESPOSTA ");
                                            break;
                                            
                                            default:
                                                break;
                                            }
                                        }
                                        printf("\n");
                                    }
                                }

                            }

                        }

                        else {
                                printf("ERROR: Envio de comando errado por parte da vizinho interno");
                        }

                        // next_newline is 
                        next_newline = strchr(neigh_addr_fd[i].buffer + next_newline, '\n') - neigh_addr_fd[i].buffer + 1;

                    }

                    if (seperated == 1) {
                        // copy the rest of the command to the beginning of the buffer
                        // copy first to an auxiliary buffer to avoid colisions while copying
                        char aux_buffer[NEIGH_BUFF_SIZE];
                        snprintf(aux_buffer, n - last_newline, "%s", neigh_addr_fd[i].buffer + last_newline);
                        snprintf(neigh_addr_fd[i].buffer, n - last_newline, "%s", aux_buffer);
                        neigh_addr_fd[i].buffer_len = n - last_newline;
                        seperated = 0;
                    }

                    if (select_cntr == 0) {
                        break;
                    }
                }
            }

        }

    }

    return 0;

}
