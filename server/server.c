#include <stdio.h>
#include <string.h>

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <signal.h>
#include <pthread.h>

#include "../utils/logging.h"
#include "udp.h"

#include "server.h"


/**
* Main UDP socket serving loop
*/
void *serve_udp_connections(void *arg) {
    LOG_DEBUG("entered serve_udp_connections");
    char *port = (char *)arg;

    int fd; // UDP socket
    struct sockaddr_in client_addr, server_addr;
    socklen_t client_addr_size = sizeof(client_addr);

    // initialize UDP socket
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        LOG_ERROR("socket: %s", strerror(errno));
        LOG_DEBUG("failed initializing UDP socket, fatal...");
        exit(1);
    }

    server_addr.sin_family      = AF_INET;
    // atoi converts string to int, htons converts int bytes in C to bytes in network notation (big endian) 
    server_addr.sin_port        = htons(atoi(port));
    server_addr.sin_addr.s_addr = INADDR_ANY; // bind to all interfaces

    if ((bind(fd, (struct sockaddr *)&server_addr, sizeof(server_addr))) != 0) {
        LOG_ERROR("bind: %s", strerror(errno));
        LOG_DEBUG("failed binding UDP socket, fatal...");
        exit(1);
    }

    LOG("Serving UDP on port %s", port);

    /**
    Main loop for UDP socket
    */
    struct udp_client udp_client;
    char client_ipv4[INET_ADDRSTRLEN];

    char recv_buffer[UDP_DATAGRAM_SIZE];
    char send_buffer[UDP_DATAGRAM_SIZE];
    size_t response_size;
    while (1) {
        memset(&udp_client, 0, sizeof(struct udp_client));

        LOG_DEBUG("blocking on recvfrom");
        int read = recvfrom(fd, recv_buffer, sizeof(recv_buffer), 0, (struct sockaddr*)&client_addr, &client_addr_size);
        LOG_DEBUG("unblocked on recvfrom");
        if (read < 0) {
            LOG_ERROR("recvfrom: %s", strerror(errno));
            LOG_DEBUG("failed reading from UDP socket, fatal...");
            exit(1);
        }

        // copy ipv4 string into client_ipv4 string variable
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ipv4, INET6_ADDRSTRLEN);

        // initialize client struct
        strcpy(udp_client.ipv4, client_ipv4);
        udp_client.addr = &client_addr;
        udp_client.addr_len = client_addr_size; 

        // if no '\n' in input, then it is wrong
        int endl_idx = strcspn(recv_buffer, "\n");
        if (endl_idx == UDP_DATAGRAM_SIZE) {
            LOG_VERBOSE("%s - sent too large message, ignoring request", udp_client.ipv4);
            continue;
        }

        recv_buffer[endl_idx] = '\0';

        LOG_DEBUG("serving %s", udp_client.ipv4);
        int err = serve_udp_command(recv_buffer, &udp_client, send_buffer, &response_size);
        // we couldn't understand the command
        if (err) {
            strcpy(send_buffer, "ERR\n");
            response_size = 4;
        }

        // command was processed, send the response
        LOG_DEBUG("%s", send_buffer);
        if (sendto(fd, send_buffer, response_size, 0, (struct sockaddr *)udp_client.addr, udp_client.addr_len) < 0) {
            LOG_ERROR("sendto: %s", strerror(errno))
            LOG_DEBUG("failed responding to client...");
        }
    }
}


/**
Main TCP socket serving loop
*/
void *serve_tcp_connections(void *arg) {
    LOG_DEBUG("entered serve_tcp_connections");
    char *port = (char *)arg;
    int fd;
    struct sockaddr_in client, server;

    if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    server.sin_family      = AF_INET;
    // atoi converts string to int, htons converts int bytes in C to bytes in network notation (big endian) 
    server.sin_port        = htons(atoi(port));
    server.sin_addr.s_addr = INADDR_ANY;

    if ((bind(fd, (struct sockaddr *)&server, sizeof(server))) != 0) {
        perror("bind");
        exit(1);
    }

    if ((listen(fd, 5)) == -1) {
        perror("listen");
        exit(1);
    }

    LOG("Serving TCP on port %s", port);

    char buffer[1024];
    char conn_fd;
    socklen_t client_addr_size = sizeof(client);
    while (1) {
        if ((conn_fd = accept(fd, (struct sockaddr*) &client, &client_addr_size)) < 0) {
            perror("accept");
            continue;
        }

        int read = recv(conn_fd, buffer, sizeof(buffer), 0);
        if (read < 0) {
            perror("recv");
            exit(1);
        }
        buffer[read] = '\0';

        int written = send(conn_fd, "hello\n", 6, 0);
        printf("sent %d\n", written);
        if (written < 0) {
            perror("send");
            exit(1);
        }

        close(conn_fd);
    }
}


void server(char *port) {
    LOG_DEBUG("entered");
    pthread_t tid[2]; // thread for UDP and thread for TCP

    // ignore SIGPIPE handler
    if (signal(SIGPIPE, SIG_IGN) != 0) {
        LOG_DEBUG("failed");
        LOG_ERROR("signal: %s", strerror(errno));
        exit(1);
    }

    // launch udp server thread
    if (pthread_create(&tid[0], NULL, serve_udp_connections, (void *)port)) {
        LOG_DEBUG("failed");
        LOG_ERROR("failed creating udp server thread");
        exit(1);
    }

    // launch tcp server thread
    if (pthread_create(&tid[1], NULL, serve_tcp_connections, (void *)port)) {
        LOG_DEBUG("failed");
        LOG_ERROR("failed creating tcp server thread");
        exit(1);
    }

    // block here forever
    for (int i = 0; i < 2; i++) {
        if (pthread_join(tid[i], NULL) != 0) {
            LOG_DEBUG("failed");
            LOG_ERROR("failed joining threads");
            exit(1);
        }
    }
}