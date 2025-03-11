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
#include "../inc/ndn.h"

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

int main(int argc, char *argv[]) {

    // own node
    Node *node;
    Node_Addr own_addr;

    int cache_size;
    int abvr = 0; // abreviation of commands

    // TCP
    int in_tcpsock_fd = 0, out_tcpsock_fd = 0, newsock_fd = 0;
    char tcp_buffer[TCP_BUFF_SIZE], in_tcp_buffer[TCP_BUFF_SIZE];
    int intr_fd[MAX_INTR];
    int num_intr = 0;

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

    // check if IP is a valid IPv4 address
    if (inet_pton(AF_INET, argv[2], own_addr.ip) != 1) {
        error("ERROR: Argumento inválido <IP> não é um endereço IPv4 válido");
    }
    strncpy(own_addr.ip, argv[2], IP_LEN);

    // check if TCP is a valid port number
    if (atoi(argv[3]) > 65535 || atoi(argv[3]) < 1024) {
        error("ERROR: Argumento inválido <TCP> não é um número de porta válido");
    }
    strncpy(own_addr.port, argv[3], PORT_LEN);

    if (argc == 6) {
        // check if IP is a valid IPv4 address
        if (inet_pton(AF_INET, argv[4], server_addr.ip) != 1) {
            error("ERROR: Argumento inválido <regIP> não é um endereço IPv4 válido");
        }
        strncpy(server_addr.ip, argv[4], IP_LEN);

        // check if TCP is a valid port number
        if (atoi(argv[5]) > 65535 || atoi(argv[5]) < 1024) {
            error("ERROR: Argumento inválido <regUDP> não é um número de porta válido");
        }
        strncpy(server_addr.port, argv[5], PORT_LEN);

    }

    else {
        // default
        strncpy(server_addr.ip, "193.136.138.142", 16);
        strncpy(server_addr.port, "59000", 6);

    }

    print_help();

    while (1) {

        // reset descriptor set
        FD_ZERO(&set_fd);
        // set stdin in descriptor set
        FD_SET(STDIN, &set_fd);

        // check max descriptor
        max_fd = max(max_fd, STDIN);

        if (in_tcpsock_fd) {
            // set in_tcpsock_fd in descriptor set
            FD_SET(in_tcpsock_fd, &set_fd);
            // repeat...
            max_fd = max(max_fd, in_tcpsock_fd);
        }

        if (out_tcpsock_fd) {
            // set out_tcpsock_fd in descriptor set
            FD_SET(out_tcpsock_fd, &set_fd);
            max_fd = max(max_fd, out_tcpsock_fd);
        }

        for (int i = 0; i < num_intr; i++) {
            // set intr_fd[i] in descriptor set
            FD_SET(intr_fd[i], &set_fd);
            max_fd = max(max_fd, intr_fd[i]);
        }

        select_cntr = select(max_fd + 1, &set_fd, NULL, NULL, NULL);

        // STDIN
        if (FD_ISSET(STDIN, &set_fd)) {

            // decrement select counter
            select_cntr--;
            // read stdin
            memset(stdin_buffer, 0, STDIN_BUFF_SIZE); // acho que dá para tirar isto
            fgets(stdin_buffer, STDIN_BUFF_SIZE, stdin);

            if ((abvr = 0, strncmp(stdin_buffer, "join ", 5) == 0) || (abvr = 1, strncmp(stdin_buffer, "j ", 2) == 0)) {
                // join
                int errcode, n, len;

                char connectIP[IP_LEN];
                char connectTCP[PORT_LEN];

                struct addrinfo *res;

                if (abvr) {   //tirar o valor da net
                    // abreviation
                    sscanf(stdin_buffer, "j %3s", net);
                } else {
                    // no abreviation
                    sscanf(stdin_buffer, "join %3s", net);
                }

                //UDP socket
                udpsock_fd = socket(AF_INET, SOCK_DGRAM, 0);
                    if(udpsock_fd == -1) error("ERROR: socket falhou");  //Error

                struct addrinfo hints;

                memset(&hints, 0, sizeof(hints));
                hints.ai_family = AF_INET;         //IPv4
                hints.ai_socktype = SOCK_DGRAM;    //UDP socket

                // get address info on node server
                errcode = getaddrinfo(server_addr.ip, server_addr.port, &hints, &res_udp);
                    if(errcode != 0) error("ERROR: getaddrinfo falhou");    //Error
                // get nodes in net
                len = snprintf(udp_buffer, UDP_BUFF_SIZE, "NODES %s", net);    // mandar as net para o buffer
                n = sendto(udpsock_fd, udp_buffer, len, 0, res_udp->ai_addr, res_udp->ai_addrlen);
                    if( n == -1)  error("ERROR: sendto falhou");   //Error
                // nodes list
                n = recvfrom(udpsock_fd, udp_buffer, UDP_BUFF_SIZE, 0, NULL, NULL);
                    if (n == -1)  error("ERROR: recvfrom falhou");   //Error

                memset(&hints, 0, sizeof(hints));
                hints.ai_family = AF_INET;          // IPv4
                hints.ai_socktype = SOCK_STREAM;    // TCP
                hints.ai_flags = AI_PASSIVE;        // Server

                // listen TCP socket
                in_tcpsock_fd = socket(AF_INET, SOCK_STREAM, 0);
                if (in_tcpsock_fd == -1) {
                    error("ERROR: socket falhou");
                }
                // get own address info
                errcode = getaddrinfo(NULL, own_addr.port, &hints, &res);
                if (errcode != 0) {
                    error("ERROR: getaddrinfo falhou");
                }
                // bind TCP socket
                errcode = bind(in_tcpsock_fd, res->ai_addr, res->ai_addrlen);
                if (errcode == -1) {
                    error("ERROR: bind falhou");
                }
                // start listening
                errcode = listen(in_tcpsock_fd, 5);
                if (errcode == -1) {
                    error("ERROR: listen falhou");
                }

                // next '\n'
                char *tok_udp = strchr(udp_buffer, '\n');
                // last '\n'
                char *term_udp = strrchr(udp_buffer, '\n');

                // create own node (address already filled)
                node = node_create();

                if (tok_udp == term_udp) {
                    // no nodes in server
                    // set safe as own
                    node_set_safe(node, own_addr);
                    node_set_ext(node, own_addr);
                    
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
                    out_tcpsock_fd = socket(AF_INET, SOCK_STREAM, 0);
                    if (out_tcpsock_fd == -1) {
                        error("ERROR: socket falhou");
                    }

                    memset(&hints, 0, sizeof(hints));
                    hints.ai_family = AF_INET;          // IPv4
                    hints.ai_socktype = SOCK_STREAM;    // TCP

                    // get external neighbor's address info
                    errcode = getaddrinfo(connectIP, connectTCP, &hints, &res);
                    if (errcode != 0) {
                        error("ERROR: getaddrinfo falhou");
                    }
                    // connect to neighbor
                    errcode = connect(out_tcpsock_fd, res->ai_addr, res->ai_addrlen);
                    if (errcode == -1) {
                        error("ERROR: connect falhou");
                    }

                    Node_Addr connect_addr;
                    strncpy(connect_addr.ip, connectIP, IP_LEN);
                    strncpy(connect_addr.port, connectTCP, PORT_LEN);

                    // set connecting node as external
                    node_set_ext(node, connect_addr);
                    // send ENTRY
                    len = snprintf(tcp_buffer, TCP_BUFF_SIZE, "ENTRY %s %s\n", own_addr.ip, own_addr.port);
                    n = write(out_tcpsock_fd, tcp_buffer, len);
                        if (n == -1) exit(1);
                }

                // register node
                len = snprintf(udp_buffer, UDP_BUFF_SIZE, "REG %s %s %s", net, own_addr.ip, own_addr.port); 
                n = sendto(udpsock_fd, udp_buffer, len, 0, res_udp->ai_addr, res_udp->ai_addrlen);
                    if( n == -1)  error("ERROR: sendto falhou");   //Error
                n = recvfrom(udpsock_fd, udp_buffer, UDP_BUFF_SIZE, 0, NULL, NULL);
                    if (n == -1)  error("ERROR: recvfrom falhou");   //Error
                if (strncmp(udp_buffer, "OKREG", 5) != 0)
                    error("ERROR: registo de nó no servidor de nós falhou");
                else
                    registered = 1;

                // free res
                freeaddrinfo(res);

            } else if ((abvr = 0, strncmp(stdin_buffer, "direct join ", 12) == 0) || (abvr = 1, strncmp(stdin_buffer, "dj ", 3) == 0)) {
                // direct join
                char connectIP[IP_LEN];
                char connectTCP[PORT_LEN];
                struct addrinfo hints, *res;
                int errcode, n, len;

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

                if (atoi(connectTCP) > 65535 || atoi(connectTCP) < 1024) {
                    fprintf(stderr, "ERROR: Argumento inválido <connectTCP> não é um número de porta válido");
                    continue;
                };

                memset(&hints, 0, sizeof(hints));
                hints.ai_family = AF_INET;          // IPv4
                hints.ai_socktype = SOCK_STREAM;    // TCP
                hints.ai_flags = AI_PASSIVE;        // Server

                // listening TCP socket
                in_tcpsock_fd = socket(AF_INET, SOCK_STREAM, 0);
                if (in_tcpsock_fd == -1) {
                    error("ERROR: socket falhou");
                }
                // get own address info
                errcode = getaddrinfo(NULL, own_addr.port, &hints, &res);
                if (errcode != 0) {
                    error("ERROR: getaddrinfo falhou");
                }
                // bind TCP socket
                errcode = bind(in_tcpsock_fd, res->ai_addr, res->ai_addrlen);
                if (errcode == -1) {
                    error("ERROR: bind falhou");
                }
                // start listening
                errcode = listen(in_tcpsock_fd, 5);
                if (errcode == -1) {
                    error("ERROR: listen falhou");
                }
                // create own node (address already filled)
                node = node_create();

                if (strcmp(connectIP, "0.0.0.0") == 0) {
                    // create network with only this node

                    // set safe as own
                    node_set_safe(node, own_addr);
                    node_set_ext(node, own_addr);
                    
                } else {
                    // connect to node

                    // external neighbor TCP socket
                    out_tcpsock_fd = socket(AF_INET, SOCK_STREAM, 0);
                    if (out_tcpsock_fd == -1) {
                        error("ERROR: socket falhou");
                    }

                    memset(&hints, 0, sizeof(hints));
                    hints.ai_family = AF_INET;          // IPv4
                    hints.ai_socktype = SOCK_STREAM;    // TCP

                    // get external neighbor address info
                    errcode = getaddrinfo(connectIP, connectTCP, &hints, &res);
                    if (errcode != 0) {
                        error("ERROR: getaddrinfo falhou");
                    }
                    // connect to external neighbor
                    errcode = connect(out_tcpsock_fd, res->ai_addr, res->ai_addrlen);
                    if (errcode == -1) {
                        error("ERROR: connect falhou");
                    }

                    // free res
                    freeaddrinfo(res);

                    Node_Addr connect_addr;
                    strncpy(connect_addr.ip, connectIP, IP_LEN);
                    strncpy(connect_addr.port, connectTCP, PORT_LEN);

                    // set external neighbor
                    node_set_ext(node, connect_addr);
                    // send ENTRY
                    len = snprintf(tcp_buffer, TCP_BUFF_SIZE, "ENTRY %s %s\n", own_addr.ip, own_addr.port);
                    n = write(out_tcpsock_fd, tcp_buffer, len);
                    if (n == -1) {
                        error("ERROR: write falhou");
                    }

                }
                
            } else if (strncmp(stdin_buffer, "create", 6) == 0 || strncmp(stdin_buffer, "c", 1) == 0) {
                // create
            } else if (strncmp(stdin_buffer, "delete", 6) == 0 || strncmp(stdin_buffer, "dl", 2) == 0) {
                // delete
            } else if (strncmp(stdin_buffer, "retrieve", 8) == 0 || strncmp(stdin_buffer, "r", 1) == 0) {
                // retrieve
            } else if (strncmp(stdin_buffer, "show topology", 13) == 0 || strncmp(stdin_buffer, "st", 2) == 0) {
                // show topology
                if (node == NULL)
                    fprintf(stderr, "ERROR: Nó não criado\n");
                else
                    print_node(node);

            } else if (strncmp(stdin_buffer, "show names", 10) == 0 || strncmp(stdin_buffer, "sn", 2) == 0) {
                // show names
            } else if (strncmp(stdin_buffer, "show interest table", 19) == 0 || strncmp(stdin_buffer, "si", 2) == 0) {
                // show interest table
            } else if (strncmp(stdin_buffer, "leave", 5) == 0 || strncmp(stdin_buffer, "l", 1) == 0) {
                // leave
            } else if (strncmp(stdin_buffer, "exit", 4) == 0 || strncmp(stdin_buffer, "x", 1) == 0) {
                // exit
                int n, len;

                if (node)
                    // was allocated
                    node_destroy(node);

                // unregister node
                if (registered) {
                    len = snprintf(udp_buffer, UDP_BUFF_SIZE, "UNREG %s %s %s", net, own_addr.ip, own_addr.port); 
                    n = sendto(udpsock_fd, udp_buffer, len, 0, res_udp->ai_addr, res_udp->ai_addrlen);
                        if( n == -1)  error("ERROR: sendto falhou");   //Error
                    n = recvfrom(udpsock_fd, udp_buffer, UDP_BUFF_SIZE, 0, NULL, NULL);
                        if (n == -1)  error("ERROR: recvfrom falhou");   //Error
                    if (strncmp(udp_buffer, "OKUNREG", 7) != 0)
                        error("ERROR: cancelamento de registo de nó no servidor de nós falhou");
                }

                if (res_udp)
                    // was allocated
                    freeaddrinfo(res_udp);

                // close descriptors
                if (in_tcpsock_fd)
                    // was open
                    close(in_tcpsock_fd);

                if (out_tcpsock_fd)
                    // was open
                    close(out_tcpsock_fd);

                if (udpsock_fd)
                    // was open
                    close(udpsock_fd);

                for (int i = 0; i < num_intr; i++)
                    close(intr_fd[i]);
                
            
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
        if (FD_ISSET(in_tcpsock_fd, &set_fd)) {

            // decrement select counter
            select_cntr--;
            // accept new TCP connection and get new socket descriptor
            newsock_fd = accept(in_tcpsock_fd, NULL, NULL);
            if (newsock_fd == -1) {
                error("ERROR: accept falhou");
            }

            if (num_intr == MAX_INTR) {
                // full of internal neighbors
                error("ERROR: Número máximo de nós internos atingido");
            }
            // save socket descriptor
            intr_fd[num_intr] = newsock_fd;
            num_intr++;

            if (select_cntr == 0) {
                continue;
            }

        }

        // TCP OUT
        if (FD_ISSET(out_tcpsock_fd, &set_fd)) {

            int n, n_aux;
            unsigned long int len = 0;
            // decrement select counter
            select_cntr--;

            n = read(out_tcpsock_fd, in_tcp_buffer, TCP_BUFF_SIZE);
            if (n == -1)
                error("ERROR: read falhou");

            while (len < (unsigned int) n) {

                if (strncmp(in_tcp_buffer + len, "ENTRY", 5) == 0) {
                    // ENTRY
                    Node_Addr intr_addr;
                    Node_Addr ext_addr;
                    // get internal address
                    sscanf(in_tcp_buffer + len, "ENTRY %s %s\n", intr_addr.ip, intr_addr.port);
                    // set internal
                    node_incr_intr(node, intr_addr);
                    // my external...
                    ext_addr = node_get_ext(node);
                    // ...his safeguard -> send SAFE
                    n_aux = snprintf(tcp_buffer, TCP_BUFF_SIZE, "SAFE %s %s\n", ext_addr.ip, ext_addr.port);
                    n_aux = write(out_tcpsock_fd, tcp_buffer, n_aux);
                    if (n == -1) {
                        error("ERROR: write falhou");
                    }

                } else if (strncmp(in_tcp_buffer + len, "SAFE", 4) == 0) {
                    // SAFE
                    Node_Addr safe_addr;
                    // get safeguard
                    sscanf(in_tcp_buffer + len, "SAFE %s %s\n", safe_addr.ip, safe_addr.port);
                    // set safe
                    node_set_safe(node, safe_addr);

                } else {
                    fprintf(stderr, "ERROR: Envio de comando errado por parte da vizinho externo\n");
                }

                len = strchr(in_tcp_buffer + len, '\n') - in_tcp_buffer + 1;
            }

            if (select_cntr == 0) {
                continue;
            }
        
        }

        // CHECK INTERNAL NEIGHBORS
        for (int i = 0; i < num_intr; i++) {
            if (FD_ISSET(intr_fd[i], &set_fd)) {
                // descriptor UP for reading

                // decrement select counter
                select_cntr--;

                int len;
                // read from descriptor
                int n = read(intr_fd[i], tcp_buffer, TCP_BUFF_SIZE);
                if (n == -1) {
                    error("ERROR: read falhou");
                }

                if (strncmp(tcp_buffer, "ENTRY", 5) == 0) {
                    // ENTRY
                    Node_Addr intr_addr;
                    Node_Addr ext_addr;
                    // get internal address
                    sscanf(tcp_buffer, "ENTRY %s %s\n", intr_addr.ip, intr_addr.port);
                    // set internal
                    node_incr_intr(node, intr_addr);

                    if (is_node_ext_own(node)) {
                        // don't have an external neighbor
                        // (ext is own by default)
                        node_set_ext(node, intr_addr);
                        ext_addr = node_get_ext(node);
                        // send SAFE (1st answer)
                        len = snprintf(tcp_buffer, TCP_BUFF_SIZE, "SAFE %s %s\n", ext_addr.ip, ext_addr.port);
                        // send ENTRY (2nd send my own command)
                        len += snprintf(tcp_buffer + len, TCP_BUFF_SIZE, "ENTRY %s %s\n", own_addr.ip, own_addr.port);
                        n = write(intr_fd[i], tcp_buffer, len);
                        if (n == -1) {
                            error("ERROR: write falhou");
                        }

                    }

                    else {
                        // ext is not own
                        ext_addr = node_get_ext(node);
                        // send SAFE
                        len = snprintf(tcp_buffer, TCP_BUFF_SIZE, "SAFE %s %s\n", ext_addr.ip, ext_addr.port);
                        n = write(intr_fd[i], tcp_buffer, len);
                        if (n == -1) {
                            error("ERROR: write falhou");
                        }

                    }
                } else if (strncmp(tcp_buffer, "SAFE", 4) == 0) {
                    // SAFE
                    Node_Addr safe_addr;
                    // get safeguard address
                    sscanf(tcp_buffer, "SAFE %s %s\n", safe_addr.ip, safe_addr.port);
                    // set safeguard
                    node_set_safe(node, safe_addr);

                } else {
                        error("ERROR: Envio de comando errado por parte da vizinho interno");
                    }

                if (select_cntr == 0) {
                    break;
                }
            }

        }

    }

    return 0;

}
