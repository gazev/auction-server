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
#include "tasks_queue.h"

#include "database.h"
#include "udp.h"
#include "tcp.h"

static pthread_t worker_threads[THREAD_POOL_SIZE];

/**
* Main UDP socket serving loop
*/
void *udp_server_thread_fn(void *arg) {
    LOG_DEBUG("entered serve_udp_connections");
    thread_t *thread = arg;
    char *port = thread->args;

    /**
    * Set socket for UDP server
    */
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
    * Main loop for UDP server 
    */
    struct udp_client udp_client;
    char client_ipv4[INET_ADDRSTRLEN];

    char recv_buffer[UDP_DATAGRAM_SIZE]; // to where we read requests 
    char send_buffer[UDP_DATAGRAM_SIZE]; // where responses are stored before sending
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

        /**
        * Perform basic message validation
        */
        if (read > 20) { // check for messages too long (for the current protocol) 
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

        /**
        * Handle the request 
        */
        LOG_DEBUG("serving %s", udp_client.ipv4);
        int err = handle_udp_command(recv_buffer, &udp_client, send_buffer, &response_size);
        if (err) { // the message didn't pass further validation steps 
            strcpy(send_buffer, "ERR\n");
            response_size = 4;
        }

        /**
        * Respond to client
        */
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
    thread_t *thread = arg;

    // retrieve arguments
    char *port           = ((struct tcp_server_thread_arg *)thread->args)->port;
    tasks_queue *tasks_q = ((struct tcp_server_thread_arg *)thread->args)->tasks_queue;

    /**
    * Set socket for TCP server
    */
    int server_sock;
    struct sockaddr_in client_addr, server_addr;

    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        LOG_ERROR("socket: %s", strerror(errno));
        LOG_DEBUG("failed setting up TCP socket");
        exit(1);
    }

    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(atoi(port)); // convert port str to necessary format
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

    LOG("Listening TCP on port %s", port);

    /**
    * Main loop for TCP server (delegating to worker threads)
    */
    char client_ipv4[INET_ADDRSTRLEN];
    struct tcp_client tcp_client;

    char conn_fd;
    int  pid;
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

        if (enqueue(tasks_q, &tcp_client) != 0) {
            LOG_DEBUG("failed enqeueing client %s, ignoring connection", tcp_client.ipv4);
            continue;
        }
    }
}

/**
* Worker threads that handle a TCP client connection
*/
void *tcp_worker_thread_fn(void *arg) {
    thread_t *thread = (thread_t *) arg;
    LOG_DEBUG("launched tcp worker thread nr %d, (tid %lu)", thread->thread_nr, thread->tid);

    tasks_queue *q = thread->args;

    task_t task;
    while (1) {
        // get work from task queue
        if (dequeue(q, &task) != 0) {
            LOG_ERROR("failed retrieving task from queue");
            continue;
        }
        // do work
        if (serve_tcp_connection(&task) != 0) {
            LOG_DEBUG("failed serving tcp client");
        };
    }
}


void server(char *port) {
    LOG_DEBUG("entered server");
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
        
    /**
    * Launch all server threads. 
    * 1 thread receiving and responding to UDP messages 
    * 1 thread accepting TCP connections
    * 20 threads handling TCP connections (THREAD_POOL_SIZE = 20)
    */
    thread_t udp_thread;
    thread_t tcp_thread;
    thread_t threads[THREAD_POOL_SIZE];

    tasks_queue *tasks_q; // producer consumer queue
    //  producer consumer queue
    if (init_queue(&tasks_q) != 0) {
        LOG_DEBUG("failed initializing tasks queue");
        exit(1);
    }

    // launch tcp workers thread pool
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        threads[i].thread_nr = i;
        threads[i].args = tasks_q;
        if (pthread_create(&threads[i].tid, NULL, tcp_worker_thread_fn, (void *)&threads[i]) != 0) {
            LOG_DEBUG("failed launching thread pool");
            LOG_ERROR("pthread_create: %s", strerror(errno));
            exit(1);
        };
    }

    // launch UDP server thread
    udp_thread.args = port;
    if (pthread_create(&udp_thread.tid, NULL, udp_server_thread_fn, (void *)&udp_thread)) {
        LOG_DEBUG("failed");
        LOG_ERROR("failed creating udp server thread");
        exit(1);
    }

    // launch TCP server thread
    struct tcp_server_thread_arg tcp_args = {
        .port = port,
        .tasks_queue = tasks_q,
    };

    tcp_thread.args = &tcp_args;
    if (pthread_create(&tcp_thread.tid, NULL, tcp_server_thread_fn, (void *)&tcp_thread)) {
        LOG_DEBUG("failed");
        LOG_ERROR("failed creating tcp server thread");
        exit(1);
    }

    /** 
    * Block here forever, no exit mechanism was specified 
    */
    if (pthread_join(udp_thread.tid, NULL) != 0) {
        LOG_DEBUG("failed joining UDP thread")
        LOG_ERROR("pthread_join: %s", strerror(errno));
        exit(1);
    }

    if (pthread_join(tcp_thread.tid, NULL) != 0) {
        LOG_DEBUG("failed joining UDP thread")
        LOG_ERROR("pthread_join: %s", strerror(errno));
        exit(1);
    }

    // block here forever
    for (int i = 0; i < 2; i++) {
        if (pthread_join(threads[i].tid, NULL) != 0) {
            LOG_DEBUG("failed");
            LOG_ERROR("failed joining threads");
            exit(1);
        }
    }


    exit(0);
}