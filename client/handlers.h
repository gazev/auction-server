#ifndef __HANDLER_H__
#define __HANDLER_H__

#include <string.h>

#include "client.h"
#include "udp.h"
#include "tcp.h"


int send_udp_request(char *, size_t, struct client_state *);
int receive_udp_response(char *, size_t, struct client_state *);

int handle_cmd(char *input, struct client_state *, char *response);

int handle_login(char *input, struct client_state *, char [MAX_SERVER_RESPONSE]);
int handle_logout(char *input, struct client_state *, char [MAX_SERVER_RESPONSE]);
int handle_unregister(char *input, struct client_state *, char [MAX_SERVER_RESPONSE]);
int handle_exit(char *input, struct client_state *, char [MAX_SERVER_RESPONSE]);
int handle_open(char *input, struct client_state *, char [MAX_SERVER_RESPONSE]);
int handle_close(char *input, struct client_state *, char [MAX_SERVER_RESPONSE]);
int handle_my_auctions(char *input, struct client_state *, char [MAX_SERVER_RESPONSE]);
int handle_my_bids(char *input, struct client_state *, char [MAX_SERVER_RESPONSE]);
int handle_list(char *input, struct client_state *, char [MAX_SERVER_RESPONSE]);
int handle_show_asset(char *input, struct client_state *, char [MAX_SERVER_RESPONSE]);
int handle_bid(char *input, struct client_state *, char [MAX_SERVER_RESPONSE]);
int handle_show_record(char *input, struct client_state *, char [MAX_SERVER_RESPONSE]);
int handle_clear(char *input, struct client_state *, char *response);
int handle_help(char *input, struct client_state *, char *response);

#endif