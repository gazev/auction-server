#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../utils/logging.h"
#include "parser.h"


/**
    Returns a command struct with operaition string and arguments
*/
struct command *parse_command(char *command) {
    LOG_DEBUG("entered parse_command");
    struct command *cmd = malloc(sizeof(*cmd));
    cmd->op = strtok(command, " ");
    cmd->args = NULL;

    // no command specified
    if (cmd->op == NULL) {
        return cmd;
    }

    struct arg *arg = malloc(sizeof(struct arg));
    cmd->args = arg;

    arg->value = strtok(NULL, " ");

    while (arg->value != NULL) {
        struct arg *next_arg = malloc(sizeof(struct arg));
        arg->next_arg = next_arg;
        arg = next_arg;
        arg->value = strtok(NULL, " ");
    }

    return cmd;
}

/**
Free memory associated to command
*/
void free_command(struct command *cmd) {
    LOG_DEBUG("entered free_command");
    struct arg *curr = cmd->args;

    while (curr != NULL) {
        struct arg *next = curr->next_arg;
        free(curr);
        curr = next;
    }

    free(cmd);
}