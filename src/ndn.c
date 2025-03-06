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

    Node *node;
    Node_Addr own_addr;

    struct timeval tv;

    char local_ip[IP_LEN];
    char local_port[PORT_LEN];
    int cache_size;

    // TCP
    int in_tcpsock_fd, out_tcpsock_fd, newsockfd;
    char tcp_buffer[TCP_BUFF_SIZE];

    int intr_fd[MAX_INTR];
    int num_intr = 0;

    // UDP
    //int udpsock_fd;
    //char udp_buffer[UDP_BUFF_SIZE];

    // STDIN
    char stdin_buffer[STDIN_BUFF_SIZE];

    int max_fd = 0;
    fd_set set_fd;
    int select_cntr;

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
    if (inet_pton(AF_INET, argv[2], local_ip) != 1) {
        error("ERROR: Argumento inválido <IP> não é um endereço IPv4 válido");
    }
    strncpy(local_ip, argv[2], IP_LEN);

    // check if TCP is a valid port number
    if (atoi(argv[3]) > 65535 || atoi(argv[3]) < 1024) {
        error("ERROR: Argumento inválido <TCP> não é um número de porta válido");
    }
    strncpy(local_port, argv[3], PORT_LEN);

    // set select(...) to watch stdin, tcpsock_fd and udpsock_fd
    FD_ZERO(&set_fd);
    FD_SET(STDIN, &set_fd);

    print_help();

    tv.tv_sec = 1;
    tv.tv_usec = 0;

    while (1) {

        select_cntr = select(max_fd + 1, &set_fd, NULL, NULL, NULL);

        // STDIN
        if (FD_ISSET(STDIN, &set_fd)) {

            select_cntr--;

            memset(stdin_buffer, 0, STDIN_BUFF_SIZE); // acho que dá para tirar isto
            fgets(stdin_buffer, STDIN_BUFF_SIZE, stdin);

            if (strncmp(stdin_buffer, "join ", 4) == 0 || strncmp(stdin_buffer, "j ", 1) == 0) {
                // join
            } else if (strncmp(stdin_buffer, "direct join ", 12) == 0 || strncmp(stdin_buffer, "dj ", 3) == 0) {
                // direct join

                char *connectIP = strtok(stdin_buffer, " ");
                connectIP = strtok(NULL, " ");
                char *connectTCP = strtok(NULL, " ");

                if (atoi(connectTCP) > 65535 || atoi(connectTCP) < 1024) {
                    fprintf(stderr, "ERROR: Argumento inválido <connectTCP> não é um número de porta válido");
                    continue;
                };

                struct addrinfo hints, *res;
                int errcode;

                memset(&hints, 0, sizeof(hints));
                hints.ai_family = AF_INET;          // IPv4
                hints.ai_socktype = SOCK_STREAM;    // TCP
                hints.ai_flags = AI_PASSIVE;        // Server

                in_tcpsock_fd = socket(AF_INET, SOCK_STREAM, 0);
                if (in_tcpsock_fd == -1) {
                    error("ERROR: socket falhou");
                }

                errcode = getaddrinfo(NULL, local_port, &hints, &res);
                if (errcode != 0) {
                    error("ERROR: getaddrinfo falhou");
                }

                errcode = bind(in_tcpsock_fd, res->ai_addr, res->ai_addrlen);
                if (errcode == -1) {
                    error("ERROR: bind falhou");
                }

                errcode = listen(in_tcpsock_fd, 5);
                if (errcode == -1) {
                    error("ERROR: listen falhou");
                }

                node = node_create();

                FD_SET(in_tcpsock_fd, &set_fd);
                if (in_tcpsock_fd > max_fd) {
                    max_fd = in_tcpsock_fd;
                }

                strncpy(own_addr.ip, local_ip, IP_LEN);
                strncpy(own_addr.port, local_port, PORT_LEN);

                if (strcmp(connectIP, "0.0.0.0") == 0) {
                    // create network with only this node

                    // set safe as own
                    node_set_safe(node, own_addr);
                    
                } else {
                    // connect to node
                    out_tcpsock_fd = socket(AF_INET, SOCK_STREAM, 0);
                    if (out_tcpsock_fd == -1) {
                        error("ERROR: socket falhou");
                    }

                    memset(&hints, 0, sizeof(hints));
                    hints.ai_family = AF_INET;          // IPv4
                    hints.ai_socktype = SOCK_STREAM;    // TCP

                    errcode = getaddrinfo(connectIP, connectTCP, &hints, &res);
                    if (errcode != 0) {
                        error("ERROR: getaddrinfo falhou");
                    }

                    errcode = connect(out_tcpsock_fd, res->ai_addr, res->ai_addrlen);
                    if (errcode == -1) {
                        error("ERROR: connect falhou");
                    }

                    FD_SET(out_tcpsock_fd, &set_fd);
                    if (out_tcpsock_fd > max_fd) {
                        max_fd = out_tcpsock_fd;
                    }

                    Node_Addr connect_addr;
                    strncpy(connect_addr.ip, connectIP, IP_LEN);
                    strncpy(connect_addr.port, connectTCP, PORT_LEN);

                    node_set_ext(node, connect_addr);

                    snprintf(tcp_buffer, TCP_BUFF_SIZE, "ENTRY %s %s\n", local_ip, local_port);
                    write(out_tcpsock_fd, tcp_buffer, strlen(tcp_buffer));
                    
                    read(out_tcpsock_fd, tcp_buffer, TCP_BUFF_SIZE);

                    Node_Addr safe_addr;
                    sscanf(tcp_buffer, "SAFE %s %s\n", safe_addr.ip, safe_addr.port);

                    node_set_safe(node, safe_addr);
                }
                
            } else if (strncmp(stdin_buffer, "create", 6) == 0 || strncmp(stdin_buffer, "c", 1) == 0) {
                // create
            } else if (strncmp(stdin_buffer, "delete", 6) == 0 || strncmp(stdin_buffer, "dl", 2) == 0) {
                // delete
            } else if (strncmp(stdin_buffer, "retrieve", 8) == 0 || strncmp(stdin_buffer, "r", 1) == 0) {
                // retrieve
            } else if (strncmp(stdin_buffer, "show topology", 13) == 0 || strncmp(stdin_buffer, "st", 2) == 0) {
                // show topology
                if (node == NULL) {
                    fprintf(stderr, "ERROR: Nó não criado\n");
                    continue;
                }

                print_node(node);
            } else if (strncmp(stdin_buffer, "show names", 10) == 0 || strncmp(stdin_buffer, "sn", 2) == 0) {
                // show names
            } else if (strncmp(stdin_buffer, "show interest table", 19) == 0 || strncmp(stdin_buffer, "si", 2) == 0) {
                // show interest table
            } else if (strncmp(stdin_buffer, "leave", 5) == 0 || strncmp(stdin_buffer, "l", 1) == 0) {
                // leave
            } else if (strncmp(stdin_buffer, "exit", 4) == 0 || strncmp(stdin_buffer, "x", 1) == 0) {
                // exit
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
            select_cntr--;

            newsockfd = accept(in_tcpsock_fd, NULL, NULL);
            if (newsockfd == -1) {
                error("ERROR: accept falhou");
            }

            if (num_intr == MAX_INTR) {
                error("ERROR: Número máximo de nós internos atingido");
            }

            intr_fd[num_intr] = newsockfd;
            num_intr++;

            FD_SET(newsockfd, &set_fd);
            if (newsockfd > max_fd) {
                max_fd = newsockfd;
            }

            printf("Novo vizinho interno\n");

            if (select_cntr == 0) {
                continue;
            }

        }

        // TCP OUT
        if (FD_ISSET(out_tcpsock_fd, &set_fd)) {

        }

        // UDP
        //if (FD_ISSET(udpsock_fd, &set_fd)) {

        //}

        // CHECK INTERNAL NEIGHBORS
        for (int i = 0; i < num_intr; i++) {
            if (FD_ISSET(intr_fd[i], &set_fd)) {
                select_cntr--;

                read(intr_fd[i], tcp_buffer, TCP_BUFF_SIZE);
                if (strncmp(tcp_buffer, "ENTRY", 5) == 0) {
                    // ENTRY
                    char *intr_ip = strtok(tcp_buffer, " ");
                    intr_ip = strtok(NULL, " ");
                    char *intr_port = strtok(NULL, " ");

                    Node_Addr intr_addr;
                    strncpy(intr_addr.ip, intr_ip, IP_LEN);
                    strncpy(intr_addr.port, intr_port, PORT_LEN);

                    node_incr_intr(node, intr_addr);

                    sprintf(tcp_buffer, "SAFE %s %s\n", local_ip, local_port);
                    write(intr_fd[i], tcp_buffer, strlen(tcp_buffer));

                } else if (strncmp(tcp_buffer, "SAFE", 4) == 0) {
                    // SAFE
                } else {
                    fprintf(stderr, "ERROR: Envio de comando errado por parte da vizinho interno\n");
                }

            }
        }

    }

    return 0;

}
