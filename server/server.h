#ifndef __SERVER_H__
#define __SERVER_H__

#include <netinet/in.h>
#include <unistd.h>

#define UDP_DATAGRAM_SIZE 65535

struct udp_client {
    char ipv4[INET_ADDRSTRLEN];
    int port;
};

struct tcp_client {
    int conn_fd;
    char ipv4[INET_ADDRSTRLEN];
    int port;
};

typedef struct thread_t {
    pthread_t tid;
    int thread_nr;
    void *args;
} thread_t;

struct tcp_server_thread_arg {
    void *tasks_queue;
    void *port;
};

void server(char *port);

#endif