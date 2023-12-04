#ifndef __COMMAND_TABLE_H__
#define __COMMAND_TABLE_H__

#include "client.h"

#define ERR_TIMEDOUT_UDP 1
#define ERR_REQUESTING_UDP 2
#define ERR_RECEIVING_UDP 3
#define ERR_NULL_ARGS 4
#define ERR_INVALID_UID 5
#define ERR_INVALID_PASSWD 6
#define ERR_INVALID_AID 7
#define ERR_UNKNOWN_ANSWER 8

typedef int (*handler_func)(char *, struct client_state *, char *);

handler_func get_handler_func(char *cmd_op);
char *get_error_msg(int errcode);

#endif 