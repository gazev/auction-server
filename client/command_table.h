#ifndef __COMMAND_TABLE_H__
#define __COMMAND_TABLE_H__

#include "parser.h"

typedef int (*handler_func)(struct arg *);

struct command_mappings {
    const char *cmd_op;
    handler_func func;
};

struct error_mappings {
    int errcode;
    char *err_msg;
};

handler_func get_handler_func(char *cmd_op);
char *get_error_msg(int errcode);

#endif 