#ifndef __UDP_COMMAND_TABLE_H__
#define __UDP_COMMAND_TABLE_H__

#include "server.h"
#include "udp.h"

#define ERR_LIN 1
#define ERR_LOU 2
#define ERR_UNR 3 
#define ERR_LMA 4
#define ERR_MB 5
#define ERR_SRC 6
#define ERR_LST 7

typedef int (*udp_handler_fn)(char *req, struct udp_client *client, char *resp, size_t *resp_len);
udp_handler_fn get_udp_handler_fn(char *cmd);
char *get_udp_error_msg(int errcode);

#endif 