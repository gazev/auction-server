#ifndef __UDP_COMMAND_TABLE_H__
#define __UDP_COMMAND_TABLE_H__

#include "server.h"
#include "udp.h"

#define ERR_LIN_BAD_VALUES 1
#define ERR_LOU_BAD_VALUES 2
#define ERR_UNR_BAD_VALUES 3 
#define ERR_LMA_BAD_VALUES 4
#define ERR_MY_BIDS_BAD_UID 5
#define ERR_SRC_BAD_VALUES 6
#define ERR_LST_BAD_VALUES 7

typedef int (*udp_handler_fn)(char *req, struct udp_client *client, char *resp, size_t *resp_len);
udp_handler_fn get_udp_handler_fn(char *cmd);
char *get_udp_error_msg(int errcode);

#endif 