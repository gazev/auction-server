#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "../utils/logging.h"
#include "parser.h"
#include "command_table.h"

/**
Handles command passed as argument.
Returns the errcode of the provided command handler. Returns 0 if operation isn't 
recognized.
*/
int handle_cmd(struct command *cmd) {
    char *cmd_op = cmd->op;

    if (cmd_op == NULL) {
        return 0;
    }

    handler_func fn = get_handler_func(cmd_op);

    if (fn == NULL) {
        LOG("Unkown command %s", cmd_op);
        return 0;
    }

    return fn(cmd->args);
}

int client(char *ip, char *port) {
    char input[MAX_COMMAND_SIZE];

    while (fgets(input, sizeof input, stdin)) {
        // put '\0' in end of line
        input[strcspn(input, "\n")] = '\0';

        struct command *cmd = parse_command(input);

        int err = handle_cmd(cmd);
        if (err) {
            LOG("%s", get_error_msg(err))
        }

        free_command(cmd);
    }

    return 0;
}

