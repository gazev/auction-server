#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/socket.h>

#include "../utils/logging.h"
#include "client.h"
#include "parser.h"
#include "command_table.h"
#include "command_error.h"

/**
Handles command passed as argument.
Returns 0 if command is successfully executed and -1 if an error occurs
*/
int handle_cmd(struct command *cmd, struct client_state *client) {
    if (cmd->op == NULL) {
        return 0;
    }

    handler_func fn = get_handler_func(cmd->op);

    if (fn == NULL) {
        LOG("Unkown command %s", cmd->op);
        return 0;
    }

    return fn(cmd->args, client);
}


/**
Still needs to be discussed
*/
// int initialize_client(char *ip, char *port, struct client_state *client) {
//     client->logged_in = 0;

//     struct addrinfo hints, *req;

//     memset(&hints, 0, sizeof(hints));
//     hints.ai_family = AF_INET;
//     hints.ai_socktype = SOCK_DGRAM;
//     hints.ai_flags = 0;

//     int err = getaddrinfo(ip, port, &hints, &req);
//     if (err) {
//         if (err == EAI_SYSTEM) {
//             LOG_ERROR("getaddrinfo: %s", strerror(errno));
//         } else {
//             LOG_ERROR("getadddrinfo: %s", gai_strerror(err));
//         }
//         return 1;
//     }

//     int announce_fd = socket(AF_INET, SOCK_DGRAM, 0);
//     if (announce_fd == -1) {
//         LOG_ERROR("socket: %s", strerror(errno));
//         return 1;
//     }

//     client->annouce_socket = announce_fd;
//     client->as_addr_len = req->ai_addrlen;
//     memcpy(&client->as_addr.sa_data, req->ai_addr->sa_data, 14);
//     client->as_addr.sa_family = req->ai_addr->sa_family;

//     return 0;
// }

// int send_message(char *message, )

void run_client(char *ip, char *port) {
    struct client_state client;

    // if (initialize_client(ip, port, &client)) {
    //     LOG_ERROR("Failed initializing client connection");
    //     exit(1);
    // }

    char input[MAX_COMMAND_SIZE];
    while (fgets(input, sizeof(input), stdin)) {
        // used to replace terminating '\n' with '\0'
        int endl_idx = strcspn(input, "\n");

        // warn user if input is too large and clean stdin unread buffer
        if (input[endl_idx] != '\n') {
            LOG_WARN("too large input, truncating");
            clean_stdin_buffer();
        }

        // replace last '\n' with '\0' (or truncate)
        input[endl_idx] = '\0';

        struct command *cmd = parse_command(input);

        int err = handle_cmd(cmd, &client);
        if (err) {
            LOG("%s", get_error_msg(err))
        }

        free_command(cmd);
    }
}

void clean_stdin_buffer() {
    char c;
    while(((c = getchar()) != EOF) && (c != '\n')); 
}
