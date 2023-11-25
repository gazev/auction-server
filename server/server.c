#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <asm-generic/socket.h>
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

#include "server.h"

#include "database.h"
#include "udp.h"
#include "tcp.h"


/**
* Main UDP socket serving loop
*/
void *udp_server_thread_fn(void *arg) {
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
        memset(&client_addr, 0, sizeof(client_addr));

        LOG_DEBUG("blocking on recvfrom");
        int read = recvfrom(fd, recv_buffer, sizeof(recv_buffer), 0, (struct sockaddr*)&client_addr, &client_addr_size);
        LOG_DEBUG("unblocked on recvfrom");
        if (read < 0) {
            LOG_ERROR("recvfrom: %s", strerror(errno));
            LOG_DEBUG("failed reading from UDP socket, fatal...");
            exit(1);
        }

        // check for bad payloads
        if (read > 20) {
            if (sendto(fd, "ERR\n", 4, 0, (struct sockaddr *)&client_addr, client_addr_size) < 0) {
                LOG_ERROR("sendto: %s", strerror(errno))
                LOG_DEBUG("failed responding to client...");
            }
            continue;
        }

        // check for message not ending in \n 
        if (recv_buffer[read - 1] != '\n') {
            if (sendto(fd, "ERR\n", 4, 0, (struct sockaddr *)&client_addr, client_addr_size) < 0) {
                LOG_ERROR("sendto: %s", strerror(errno))
                LOG_DEBUG("failed responding to client...");
            }
            continue;
        }

        // copy ipv4 string into client_ipv4 string variable
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ipv4, INET6_ADDRSTRLEN);
        // initialize client struct
        strcpy(udp_client.ipv4, client_ipv4);

        /*
        * Handle the request 
        */
        LOG_DEBUG("serving %s", udp_client.ipv4);
        int err = handle_udp_command(recv_buffer, &udp_client, send_buffer, &response_size);
        // if we couldn't understand the command
        if (err) {
            strcpy(send_buffer, "ERR\n");
            response_size = 4;
        }

        // command was processed, send the response
        printf("%s", send_buffer);
        if (sendto(fd, send_buffer, response_size, 0, (struct sockaddr *)&client_addr, client_addr_size) < 0) {
            LOG_ERROR("sendto: %s", strerror(errno))
            LOG_DEBUG("failed responding to client...");
        }
    }
}


/**
Main TCP socket serving loop
*/
void *tcp_server_thread_fn(void *arg) {
    LOG_DEBUG("entered serve_tcp_connections");
    char *port = (char *)arg;
    int server_sock;
    struct sockaddr_in client_addr, server_addr;

    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        LOG_ERROR("socket: %s", strerror(errno));
        LOG_DEBUG("failed setting up TCP socket");
        exit(1);
    }

    server_addr.sin_family      = AF_INET;
    // atoi converts string to int, htons converts int bytes in C to bytes in network notation (big endian) 
    server_addr.sin_port        = htons(atoi(port));
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if ((bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr))) != 0) {
        LOG_ERROR("bind: %s", strerror(errno));
        LOG_DEBUG("failed binding TCP socket");
        exit(1);
    }

    if ((listen(server_sock, 5)) == -1) {
        LOG_ERROR("listen: %s", strerror(errno));
        LOG_DEBUG("failed listening on TCP socket");
        exit(1);
    }

    LOG("Serving TCP on port %s", port);

    char client_ipv4[INET_ADDRSTRLEN];
    struct tcp_client tcp_client;

    char conn_fd;
    int pid;
    socklen_t client_addr_size = sizeof(client_addr);
    while (1) {
        memset(&tcp_client, 0, sizeof(struct tcp_client));
        memset(&client_addr, 0, sizeof(client_addr));

        if ((conn_fd = accept(server_sock, (struct sockaddr*) &client_addr, &client_addr_size)) < 0) {
            LOG_ERROR("accept: %s", strerror(errno));
            LOG_DEBUG("failed accepting new connection");
            continue;
        }

        // copy ipv4 string into client_ipv4 string variable
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ipv4, INET6_ADDRSTRLEN);
        // initialize client struct
        strcpy(tcp_client.ipv4, client_ipv4);
        tcp_client.conn_fd = conn_fd;

        pid = fork();
        if (pid == -1) { // error creating child
            LOG_ERROR("fork: %s", strerror(errno));
            LOG_DEBUG("failed creating child process for client %s", tcp_client.ipv4);
        } else if (pid == 0) { // child
            close(server_sock);

            struct timeval timeout;
            timeout.tv_sec = 5; // 5 sec timeout
            timeout.tv_usec = 0;

            // set timeout for socket
            if (setsockopt(conn_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
                LOG_ERROR("setsockopt: %s", strerror(errno));
                LOG_DEBUG("failed setting time out socket for client %s", tcp_client.ipv4);
                exit(1);
            }

            /**
            * Read command message from stream
            */
            char cmd_buffer[8];
            char *ptr = cmd_buffer;
            int cmd_size = 3; // command size
            int total_read = 0;
            // read protocol operation 
            while (total_read < cmd_size) {
                int read = recv(conn_fd, ptr + total_read, cmd_size - total_read, 0);

                if (read < 0) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        LOG_VERBOSE("timed out client %s", tcp_client.ipv4);
                        exit(0);
                    } else {
                        LOG_ERROR("recv: %s", strerror(errno));
                        LOG_DEBUG("failed reading from socket. client %s", tcp_client.ipv4);
                        exit(1);
                    }
                }

                if (read == 0) {
                    LOG_DEBUG("connection closed by client %s", tcp_client.ipv4);
                    close(conn_fd);
                    exit(0);
                }
                
                total_read += read;
                ptr += read;
            }

            LOG_DEBUG("serving %s", tcp_client.ipv4);
            // handle the command
            int err = handle_tcp_command(cmd_buffer, &tcp_client);
            if (err) { // if we could not understand the command
                char *err = "ERR\n";
                char *ptr = err;
                char err_len = 4;
                int total_written = 0;
                while (total_written < err_len) {
                    int written = send(conn_fd, ptr, err_len - total_written, 0);

                    if (written < 0) {
                        LOG_ERROR("write: %s", strerror(errno));
                        LOG_DEBUG("failed responding to client");
                        break;
                    }

                    total_written += written;
                }
            }

            close(conn_fd);
            exit(0);
        } else { // server proc
            close(conn_fd);
        }
    }
}


void server(char *port) {
    LOG_DEBUG("entered server");
    pthread_t tid[2]; // thread for UDP and thread for TCP

    // initialize database
    if (init_database() != 0) {
        LOG_DEBUG("failed initializing db");
        LOG_ERROR("signal: %s", strerror(errno));
    }

    // ignore SIGPIPE handler
    if (signal(SIGPIPE, SIG_IGN) != 0) {
        LOG_DEBUG("failed");
        LOG_ERROR("signal: %s", strerror(errno));
        exit(1);
    }

    // launch udp server thread
    if (pthread_create(&tid[0], NULL, udp_server_thread_fn, (void *)port)) {
        LOG_DEBUG("failed");
        LOG_ERROR("failed creating udp server thread");
        exit(1);
    }

    // launch tcp server thread
    if (pthread_create(&tid[1], NULL, tcp_server_thread_fn, (void *)port)) {
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