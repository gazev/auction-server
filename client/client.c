#include <asm-generic/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>
#include <signal.h>

#include "../utils/config.h"
#include "../utils/logging.h"
#include "../utils/constants.h"

#include "client.h"
#include "handlers.h"
#include "command_table.h"


/**
* Initialize client state structure
*/
int initialize_client(char *ip, char *port, struct client_state *client) {
    int sock_fd;
    struct addrinfo hints, *req;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = 0;

    int err = getaddrinfo(ip, port, &hints, &req);
    if (err) {
        LOG_ERROR("Failed DNS lookup");
        if (err == EAI_SYSTEM) {
            LOG_ERROR("getaddrinfo: %s", strerror(errno));
        } else {
            LOG_ERROR("getadddrinfo: %s", gai_strerror(err));
        }
        return -1;
    }

    // not sure how this could happen, but we safe guard it here
    if (req->ai_addr == NULL) {
        LOG_ERROR("Failed DNS lookup")
        return -1;
    }

    if ((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        LOG_ERROR("Failed setting up client UDP socket");
        LOG_ERROR("socket: %s", strerror(errno));
        return -1;
    }

    //set timeout to socket
    struct timeval timeout;
    timeout.tv_sec = UDP_CLIENT_TIMEOUT;  // Timeout in seconds
    timeout.tv_usec = 0; // Additional microseconds
    if (setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        LOG_DEBUG("Failed setting timeout for UDP response socket")
        LOG_DEBUG("setsockopt: %s", strerror(errno));
        return 1;
    }

    if (setsockopt(sock_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        LOG_DEBUG("Failed setting timeout for UDP response socket")
        LOG_DEBUG("setsockopt: %s", strerror(errno));
        return 1;
    }
 

    client->logged_in = 0;
    client->annouce_socket = sock_fd;
    // copy address information 
    client->as_addr_len = req->ai_addrlen;
    client->as_addr = malloc(sizeof(struct sockaddr));
    memcpy(client->as_addr, req->ai_addr, sizeof(*req->ai_addr));

    freeaddrinfo(req);

    return 0;
}

void free_client(struct client_state *client) {
    close(client->annouce_socket);
    free(client->as_addr);
}

/**
* Main client event loop
*/
void run_client(char *ip, char *port) {
    struct client_state client;

    if (initialize_client(ip, port, &client) != 0) {
        LOG_ERROR("Failed initializing client");
        exit(1);
    }

    if (signal(SIGPIPE, SIG_IGN) != 0) {
        LOG_ERROR("Failed overwriting default SIGPIPE handler");
        LOG_ERROR("signal: %s", strerror(errno));
    }

    char user_input[1024];
    char response[MAX_SERVER_RESPONSE];
    DISPLAY_CLIENT("--- Welcome to the AS client! ---\n");
    while (fgets(user_input, 1024, stdin)) {
        int endl_idx = strcspn(user_input, "\n");

        if (user_input[endl_idx] != '\n') {
            DISPLAY_CLIENT("Command too large! Type `help` for more information\n");
            clean_stdin_buffer();
            continue;
        }

        // replace last '\n' with '\0' (or truncate)
        user_input[endl_idx] = '\0';

        int err = handle_cmd(user_input, &client, response);
        if (err) {
            DISPLAY_CLIENT("%s", get_error_msg(err))
            continue;
        }

        DISPLAY_CLIENT("%s", response);
    }
}

void clean_stdin_buffer() {
    char c;
    while(((c = getchar()) != EOF) && (c != '\n')); 
}
