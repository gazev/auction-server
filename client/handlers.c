#include "handlers.h"
#include "parser.h"
#include "../utils/logging.h"
#include <stdio.h>

int handle_login (struct arg *args) {
    LOG_DEBUG(" ");
    return ERROR_LOGIN;
}

int handle_logout (struct arg *args) {
    LOG_DEBUG(" ");
    return ERROR_LOGOUT;
}

int handle_unregister (struct arg *args) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_exit (struct arg *args) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_open (struct arg *args) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_close (struct arg *args) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_my_auctions (struct arg *args) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_my_bids (struct arg *args) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_list (struct arg *args) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_show_asset (struct arg *args) {
    LOG_DEBUG("%s", args->value);
    return 0;
}

int handle_bid (struct arg *args) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_show_record (struct arg *args) {
    LOG_DEBUG(" ");
    return 0;
}

