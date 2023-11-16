#ifndef __HANDLER_H__
#define __HANDLER_H__

#include "client.h"
#include "parser.h"

#define ERROR_LOGIN 1
#define ERROR_LOGOUT 2

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