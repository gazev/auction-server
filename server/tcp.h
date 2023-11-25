#ifndef __TCP_H__
#define __TCP_H__

#include "server.h"

int handle_tcp_command(char *cmd, struct tcp_client *);

int handle_open(char *cmd, struct tcp_client *);
int handle_close(char *cmd, struct tcp_client *);
int handle_show_asset(char *cmd, struct tcp_client *);
int handle_bid(char *cmd, struct tcp_client *);

#endif