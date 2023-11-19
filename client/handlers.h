#ifndef __HANDLER_H__
#define __HANDLER_H__

#include "client.h"
#include "parser.h"

#define ERROR_LOGIN 1
#define ERROR_LOGOUT 2

#define MAX_LOGIN_COMMAND 20
#define MAX_LOGIN_RESPONSE 8
#define SUCESSFULL_LOGIN "RLI OK"
#define SUCESSFULL_REGISTER "RLI REG"
#define UNSUCESSFULL_LOGIN "RLI NOK"

#define MAX_LOGOUT_COMMAND 20
#define MAX_LOGOUT_RESPONSE 8
#define SUCESSFULL_LOGOUT "RLO OK"
#define UNREGISTERED_LOGOUT "RLO UNR"
#define UNSUCESSFULL_LOGOUT "RLO NOK"

int send_udp_request(char *, size_t, struct client_state *);
int receive_udp_response(char *, size_t, struct client_state *);

int handle_login(struct arg *, struct client_state *);
int handle_logout(struct arg *, struct client_state *);
int handle_unregister(struct arg *, struct client_state *);
int handle_exit(struct arg *, struct client_state *);
int handle_open(struct arg *, struct client_state *);
int handle_close(struct arg *, struct client_state *);
int handle_my_auctions(struct arg *, struct client_state *);
int handle_my_bids(struct arg *, struct client_state *);
int handle_list(struct arg *, struct client_state *);
int handle_show_asset(struct arg *, struct client_state *);
int handle_bid(struct arg *, struct client_state *);
int handle_show_record(struct arg *, struct client_state *);

#endif