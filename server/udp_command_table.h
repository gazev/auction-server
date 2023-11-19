#ifndef __UDP_COMMAND_TABLE_H__
#define __UDP_COMMAND_TABLE_H__

#include "server.h"
#include "udp.h"

typedef int (*udp_handler_fn)(char *cmd, struct udp_client *client);

udp_handler_fn get_udp_handler_func(char *cmd);
char *get_error_msg(int errcode);

#endif 