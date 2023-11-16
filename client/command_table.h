#ifndef __COMMAND_TABLE_H__
#define __COMMAND_TABLE_H__

#include "parser.h"
#include "client.h"

typedef int (*handler_func)(struct arg *, struct client_state *);

handler_func get_handler_func(char *cmd_op);
char *get_error_msg(int errcode);

#endif 