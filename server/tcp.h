#ifndef __TCP_H__
#define __TCP_H__

#include "server.h"

int serve_tcp_connection(struct tcp_client *);
int handle_tcp_command(char *cmd, struct tcp_client *);
int handle_open(struct tcp_client *);
int handle_close(struct tcp_client *);
int handle_show_asset(struct tcp_client *);
int handle_bid(struct tcp_client *);

#endif