#ifndef __CLIENT_H__
#define __CLIENT_H__

#include <netinet/in.h>

#define UID_SIZE 7
#define PASSWORD_SIZE 8

struct client_state {
    int logged_in;      // 1 or 0
    char uid[UID_SIZE];        // an IST number
    char passwd[PASSWORD_SIZE];     // alphanumeric string
    int annouce_socket; // UDP socket
    struct sockaddr *as_addr; // AS server socket address
    socklen_t as_addr_len;   // AS server sockaddr length
};

void run_client(char *ip, char *port);

void clean_stdin_buffer(void);

#endif