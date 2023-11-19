#ifndef __UDP_H__
#define __UDP_H__

#include "server.h"

int serve_udp_command(char *request, struct udp_client *client, char *response, size_t *response_len);

int handle_login(char *req, struct udp_client *client, char *resp, size_t *resp_len);
int handle_logout(char *req, struct udp_client *client, char *resp, size_t *resp_len);
int handle_unregister(char *req, struct udp_client *client, char *resp, size_t *resp_len);
int handle_exit(char *req, struct udp_client *client, char *resp, size_t *resp_len);
int handle_open(char *req, struct udp_client *client, char *resp, size_t *resp_len);
int handle_close(char *req, struct udp_client *client, char *resp, size_t *resp_len);
int handle_my_auctions(char *req, struct udp_client *client, char *resp, size_t *resp_len);

#endif