#ifndef __UDP_COMMAND_TABLE_H__
#define __UDP_COMMAND_TABLE_H__

#include "server.h"
#include "udp.h"

typedef int (*udp_handler_fn)(char *req, struct udp_client *client, char *resp, size_t *resp_len);

udp_handler_fn get_udp_handler_fn(char *cmd);
char *get_udp_error_msg(int errcode);

#endif 