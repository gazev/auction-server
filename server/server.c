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
#include "../utils/config.h"

#include "server.h"
#include "tasks_queue.h"

#include "database.h"
#include "udp.h"
#include "tcp.h"

/**
* Main UDP socket serving loop
*/
void *udp_server_thread_fn(void *thread_v) {
    thread_t *thread = thread_v;
    char *port = thread->args;

    /**
    * Set socket for UDP server
    */
    int udp_sock;
    struct sockaddr_in client_addr, server_addr;
    socklen_t client_addr_size = sizeof(client_addr);

    // initialize UDP socket
    if ((udp_sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        LOG_ERROR("Failed initializing UDP socket");
        LOG_ERROR("socket: %s", strerror(errno));
        exit(1);
    }

    server_addr.sin_family      = AF_INET;
    // atoi converts string to int, htons converts int bytes in C to bytes in network notation (big endian) 
    server_addr.sin_port        = htons(atoi(port));
    server_addr.sin_addr.s_addr = INADDR_ANY; // bind to all interfaces

    if ((bind(udp_sock, (struct sockaddr *)&server_addr, sizeof(server_addr))) != 0) {
        LOG_ERROR("Failed binding UDP socket");
        LOG_ERROR("bind: %s", strerror(errno));
        exit(1);
    }

    LOG("[UDP] Serving UDP connections on port %s", port);

    /**
    * Main loop for UDP server 
    */
    struct udp_client udp_client;
    char client_ipv4[INET_ADDRSTRLEN];

    char recv_buffer[UDP_DATAGRAM_SIZE]; // to where we read requests 
    char send_buffer[UDP_DATAGRAM_SIZE]; // where responses are stored before sending
    size_t response_size;
    while (1) {
        memset(recv_buffer, 0, sizeof(recv_buffer));
        memset(send_buffer, 0, sizeof(send_buffer));
        memset(&udp_client, 0, sizeof(struct udp_client));
        memset(&client_addr, 0, sizeof(client_addr));

        ssize_t read = recvfrom(udp_sock, recv_buffer, sizeof(recv_buffer), 0, (struct sockaddr*)&client_addr, &client_addr_size);
        if (read < 0) {
            LOG_DEBUG("[UDP] Failed reading from socket");
            LOG_ERROR("[UDP] recvfrom: %s", strerror(errno));
            continue;
        }

        // copy ipv4 string into client_ipv4 string variable
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ipv4, INET6_ADDRSTRLEN);
        // initialize client struct
        strcpy(udp_client.ipv4, client_ipv4);
        udp_client.port = htons(client_addr.sin_port);

        /**
        * Perform basic message validation
        */
        if (read == 0) {
            LOG_VERBOSE("%s:%d - [UDP] Empty message received", udp_client.ipv4, udp_client.port);
            if (sendto(udp_sock, "ERR\n", 4, 0, (struct sockaddr *)&client_addr, client_addr_size) < 0) {
                LOG_DEBUG("[UDP] Failed responding to client...");
                LOG_ERROR("[UDP] sendto: %s", strerror(errno))
            }
            continue;
        }

         // check for messages too long (for the current protocol) 
        if (read > 20) {
            LOG_VERBOSE("%s:%d - [UDP] Ignoring too long message", udp_client.ipv4, udp_client.port);
            if (sendto(udp_sock, "ERR\n", 4, 0, (struct sockaddr *)&client_addr, client_addr_size) < 0) {
                LOG_DEBUG("[UDP] Failed responding to client...");
                LOG_ERROR("[UDP] sendto: %s", strerror(errno))
            }
            continue;
        }

        // check for message not ending in \n 
        if (recv_buffer[read - 1] != '\n') {
            LOG_VERBOSE("%s:%d - [UDP] Ignoring badly formatted message", udp_client.ipv4, udp_client.port);
            if (sendto(udp_sock, "ERR\n", 4, 0, (struct sockaddr *)&client_addr, client_addr_size) < 0) {
                LOG_DEBUG("[UDP] Failed responding to client...");
                LOG_ERROR("sendto: %s", strerror(errno))
            }
            continue;
        }

        /**
        * Handle the request 
        */
        LOG_VERBOSE("%s:%d - [UDP] Serving client", udp_client.ipv4, udp_client.port);

        int err = handle_udp_command(recv_buffer, &udp_client, send_buffer, &response_size);
        if (err) { // the message didn't pass further validation steps 
            strcpy(send_buffer, "ERR\n");
            response_size = 4;
        }

        /**
        * Respond to client
        */
        if (sendto(udp_sock, send_buffer, response_size, 0, (struct sockaddr *)&client_addr, client_addr_size) < 0) {
            LOG_DEBUG("%s:%d - [UDP] Failed responding to client", udp_client.ipv4, udp_client.port);
            LOG_ERROR("sendto: %s", strerror(errno))
        }
    }
}


/**
Main TCP socket serving loop
*/
void *tcp_server_thread_fn(void *thread_v) {
    thread_t *thread = thread_v;

    struct tcp_server_thread_arg *args = thread->args;
    char *port           = args->port;
    tasks_queue *tasks_q = args->tasks_queue;

    // retrieve arguments

    /**
    * Set socket for TCP server
    */
    int server_sock;
    struct sockaddr_in server_addr;

    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        LOG_ERROR("[TPC] Failed setting up TCP socket");
        LOG_ERROR("[TCP] socket: %s", strerror(errno));
        exit(1);
    }

    server_addr.sin_family      = AF_INET;
    server_addr.sin_port        = htons(atoi(port)); // convert port str to necessary format
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if ((bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr))) != 0) {
        LOG_ERROR("[TCP] Failed binding TCP socket");
        LOG_ERROR("[TCP] bind: %s", strerror(errno));
        exit(1);
    }

    if ((listen(server_sock, 30)) == -1) {
        LOG_ERROR("[TPC] Failed to listen to TCP on socket");
        LOG_ERROR("[TCP] listen: %s", strerror(errno));
        exit(1);
    }

    LOG("[TCP] Listening for TCP connections on port %s", port);

    /**
    * Main loop for TCP server (delegating to worker threads)
    */
    struct tcp_client tcp_client;
    char client_ipv4[INET_ADDRSTRLEN];
    char conn_fd;

    struct sockaddr_in client_addr;
    socklen_t client_addr_size = sizeof(client_addr);
    while (1) {
        memset(&tcp_client, 0, sizeof(struct tcp_client));
        memset(&client_addr, 0, sizeof(client_addr));

        if ((conn_fd = accept(server_sock, (struct sockaddr*) &client_addr, &client_addr_size)) < 0) {
            LOG_ERROR("[TCP] Failed accepting new connection");
            LOG_DEBUG("[TPC] accept: %s", strerror(errno));
            continue;
        }

        // copy ipv4 string into client_ipv4 string variable
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ipv4, INET6_ADDRSTRLEN);

        // initialize client struct
        strcpy(tcp_client.ipv4, client_ipv4);
        tcp_client.conn_fd = conn_fd;
        tcp_client.port = htons(client_addr.sin_port); 

        if (enqueue(tasks_q, &tcp_client) != 0) {
            LOG_DEBUG("%s:%d - [TCP] Failed enqeueing client task, dropping connection", tcp_client.ipv4, tcp_client.port);
            if (close(tcp_client.conn_fd) != 0) {
                LOG_DEBUG("[TPC] Failed closing tcp_client.conn_fd");
                LOG_ERROR("[TCP] close: %s", strerror(errno));
            };
        }
    }
}

/**
* Worker threads that handle a TCP client connection
*/
void *tcp_worker_thread_fn(void *arg) {
    thread_t *thread = (thread_t *) arg;
    LOG_DEBUG("Launched TCP worker thread %02d, (tid %lu)", thread->thread_nr, thread->tid);

    tasks_queue *q = thread->args;

    task_t task;
    while (1) {
        // get work from task queue
        if (dequeue(q, &task) != 0) {
            LOG_DEBUG("[TCP] Failed retrieving task from queue");
            continue;
        }
        // do work
        LOG_DEBUG("[TCP] Worker %d handling client %s", thread->thread_nr, task.ipv4);
        if (serve_tcp_connection(&task) != 0) {
            LOG_DEBUG("[TCP] Failed serving TCP client");
        };
    }
}


void server(char *port) {
    // initialize database
    if (init_database() != 0) {
        LOG_ERROR("Failed initializing database");
        exit(1);
    }

    // ignore SIGPIPE handler
    if (signal(SIGPIPE, SIG_IGN) != 0) {
        LOG_ERROR("Failed overwritting SIGPIPE handler");
        LOG_ERROR("signal: %s", strerror(errno));
        exit(1);
    }
        
    /**
    * Launch all server threads. 
    * 1 thread receiving and responding to UDP messages 
    * 1 thread accepting TCP connections
    * 30 threads handling TCP connections (THREAD_POOL_SIZE = 20)
    */
    thread_t udp_thread;
    thread_t tcp_thread;
    thread_t worker_threads[THREAD_POOL_SZ];

    tasks_queue *tasks_q; // producer consumer queue
    //  producer consumer queue
    if (init_queue(&tasks_q) != 0) {
        LOG_ERROR("Failed initializing tasks queue");
        exit(1);
    }

    // launch tcp workers thread pool
    for (int i = 0; i < THREAD_POOL_SZ; i++) {
        worker_threads[i].thread_nr = i;
        worker_threads[i].args = tasks_q;
        if (pthread_create(&worker_threads[i].tid, NULL, tcp_worker_thread_fn, (void *)&worker_threads[i]) != 0) {
            LOG_ERROR("Failed launching worker number %02d", i);
            exit(1);
        };
    }

    // launch UDP server thread
    udp_thread.args = port;
    if (pthread_create(&udp_thread.tid, NULL, udp_server_thread_fn, (void *)&udp_thread)) {
        LOG_ERROR("Failed creating UDP server thread");
        exit(1);
    }

    // launch TCP server thread
    struct tcp_server_thread_arg tcp_args = {
        .port = port,
        .tasks_queue = tasks_q,
    };

    tcp_thread.args = &tcp_args;
    if (pthread_create(&tcp_thread.tid, NULL, tcp_server_thread_fn, (void *)&tcp_thread)) {
        LOG_ERROR("Failed creating TCP server thread");
        exit(1);
    }

    /** 
    * Block here forever, no exit mechanism was specified 
    */
    if (pthread_join(udp_thread.tid, NULL) != 0) {
        LOG_ERROR("Failed joining UDP server thread");
        exit(1);
    }

    if (pthread_join(tcp_thread.tid, NULL) != 0) {
        LOG_ERROR("Failed joining TCP server thread");
        exit(1);
    }

    for (int i = 0; i < THREAD_POOL_SZ; i++) {
        if (pthread_join(worker_threads[i].tid, NULL) != 0) {
            LOG_ERROR("Failed joining worker threads");
            exit(1);
        }
    }


    exit(0);
}