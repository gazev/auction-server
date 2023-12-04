#ifndef __HANDLER_H__
#define __HANDLER_H__

#include <string.h>

#include "client.h"

#define MAX_LOGIN_COMMAND 20
#define MAX_LOGIN_RESPONSE 8
#define SUCESSFULL_LOGIN "RLI OK\n"
#define SUCESSFULL_REGISTER "RLI REG\n"
#define UNSUCESSFULL_LOGIN "RLI NOK\n"

#define MAX_LOGOUT_COMMAND 20
#define MAX_LOGOUT_RESPONSE 8
#define SUCESSFULL_LOGOUT "RLO OK\n"
#define UNREGISTERED_LOGOUT "RLO UNR\n"
#define UNSUCESSFULL_LOGOUT "RLO NOK\n"

#define MAX_UNREGISTER_COMMAND 20
#define MAX_UNREGISTER_RESPONSE 8
#define SUCESSFULL_UNREGISTER "RUR OK\n"
#define UNREGISTERED_UNREGISTER "RUR UNR\n"
#define LOGGED_OUT_UNREGISTER "RUR NOK\n"

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

#endif