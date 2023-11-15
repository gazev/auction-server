#ifndef __HANDLER_H__
#define __HANDLER_H__

#include "parser.h"

#define ERROR_LOGIN 1
#define ERROR_LOGOUT 2

int handle_login(struct arg *);
int handle_logout(struct arg *);
int handle_unregister(struct arg *);
int handle_exit(struct arg *);
int handle_open(struct arg *);
int handle_close(struct arg *);
int handle_my_auctions(struct arg *);
int handle_my_bids(struct arg *);
int handle_list(struct arg *);
int handle_show_asset(struct arg *);
int handle_bid(struct arg *);
int handle_show_record(struct arg *);

#endif