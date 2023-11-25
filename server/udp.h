#ifndef __UDP_H__
#define __UDP_H__

#include "server.h"

int handle_udp_command(char *request, struct udp_client *client, char *response, size_t *response_len);

int handle_login(char *req, struct udp_client *client, char *resp, size_t *resp_len);
int handle_logout(char *req, struct udp_client *client, char *resp, size_t *resp_len);
int handle_unregister(char *req, struct udp_client *client, char *resp, size_t *resp_len);
int handle_my_bids(char *req, struct udp_client *client, char *resp, size_t *resp_len);
int handle_my_auctions(char *req, struct udp_client *client, char *resp, size_t *resp_len);
int handle_list(char *req, struct udp_client *client, char *resp, size_t *resp_len);
int handle_show_record(char *req, struct udp_client *client, char *resp, size_t *resp_len);

#endif