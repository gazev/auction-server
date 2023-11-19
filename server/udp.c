#include <string.h>
#include <stdio.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../utils/logging.h"
#include "server.h"
#include "udp.h"

#include "udp_command_table.h"

int serve_udp_command(char *input, struct udp_client *client) {
    LOG_DEBUG("entered serve_udp_command");
    char *cmd = strtok(input, " ");

    if (cmd == NULL) {
        LOG_DEBUG("received no command");
        LOG_VERBOSE("%s - no command supplied", client->ipv4);
        return -1;
    }

    if (strlen(cmd) != 3) {
        // printf("%ld\n", strlen(cmd));
        LOG_DEBUG("strlen(cmd) != 3, cmd = %s", cmd)
        LOG_VERBOSE("%s - ignoring invalid command %s", client->ipv4, cmd);
        return -1;
    }

    udp_handler_fn fn = get_udp_handler_func(cmd);

    if (fn == NULL) {
        LOG_DEBUG("NULL fn handler")
        LOG_VERBOSE("%s - ignoring unkown command %s", client->ipv4, cmd);
        return -1;
    }

    fn(input, client);
    return 0;
}

int handle_login(char *input, struct udp_client *client) {
    LOG_DEBUG("entered handle_login");
    return 0;
}

int handle_logout(char *input, struct udp_client *client) {
    LOG_DEBUG("entered handle_logout");
    return 0;
}

int handle_unregister(char *input, struct udp_client *client) {
    LOG_DEBUG("entered handle_unregister");
    return 0;
}

int handle_exit(char *input, struct udp_client *client) {
    LOG_DEBUG("entered handle_exit");
    return 0;
}

int handle_open(char *input, struct udp_client *client) {
    LOG_DEBUG("entered handle_open");
    return 0;
}

int handle_close(char *input, struct udp_client *client) {
    LOG_DEBUG("entered handle_close");
    return 0;
}

int handle_my_auctions(char *input, struct udp_client *client) {
    LOG_DEBUG("entered");
    return 0;
}

