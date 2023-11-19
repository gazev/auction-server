#ifndef __UDP_H__
#define __UDP_H__

#include "server.h"

int serve_udp_command(char *input, struct udp_client *client);

int handle_login(char *input, struct udp_client *client);
int handle_logout(char *input, struct udp_client *client);
int handle_unregister(char *input, struct udp_client *client);
int handle_exit(char *input, struct udp_client *client);
int handle_open(char *input, struct udp_client *client);
int handle_close(char *input, struct udp_client *client);
int handle_my_auctions(char *input, struct udp_client *client);

#endif