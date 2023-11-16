#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <netinet/in.h>

struct client_state {
    int logged_in;
    int annouce_socket; // UDP socket
    struct sockaddr as_addr;
    socklen_t as_addr_len;
};

void run_client(char *ip, char *port);

void clean_stdin_buffer(void);

#endif