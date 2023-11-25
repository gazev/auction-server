#ifndef __SERVER_H__
#define __SERVER_H__

#include <netinet/in.h>
#include <unistd.h>

#define UDP_DATAGRAM_SIZE 65535

struct udp_client {
    char ipv4[INET_ADDRSTRLEN];
};

struct tcp_client {
    int conn_fd;
    char ipv4[INET_ADDRSTRLEN];
};

void server(char *port);

#endif