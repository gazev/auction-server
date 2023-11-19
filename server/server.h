#ifndef __SERVER_H__
#define __SERVER_H__

#include <netinet/in.h>
#include <unistd.h>

#define UDP_DATAGRAM_SIZE 65535

struct udp_client {
    char ipv4[INET_ADDRSTRLEN];
    struct sockaddr_in *addr;
    socklen_t addr_len;
};

void server(char *port);

#endif