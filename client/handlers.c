#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>

#include "handlers.h"
#include "client.h"
#include "parser.h"
#include "../utils/logging.h"

int handle_login (struct arg *args, struct client_state *client) {
    if (client->logged_in) {
        LOG("already logged in");
        return 0;
    }

    char *uid, *passwd;

    // validate UID arguments here and so on
}

int handle_logout (struct arg *args, struct client_state *client) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_unregister (struct arg *args, struct client_state *client) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_exit (struct arg *args, struct client_state *client) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_open (struct arg *args, struct client_state *client) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_close (struct arg *args, struct client_state *client) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_my_auctions (struct arg *args, struct client_state *client) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_my_bids (struct arg *args, struct client_state *client) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_list (struct arg *args, struct client_state *client) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_show_asset (struct arg *args, struct client_state *client) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_bid (struct arg *args, struct client_state *client) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_show_record (struct arg *args, struct client_state *client) {
    LOG_DEBUG(" ");
    return 0;
}

