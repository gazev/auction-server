#include <stdlib.h>
#include <string.h>
#include "parser.h"

struct command *parse_command(char *command) {
    struct command *cmd = malloc(sizeof(struct command));
    cmd->op = strtok(command, " ");

    struct arg *arg = malloc(sizeof(struct arg)); 
    cmd->args = arg;

    arg->value = strtok(NULL, " ");
    while (arg->value != NULL) {
        struct arg* next_arg = malloc(sizeof(struct arg));
        arg->next_arg = next_arg;
        arg = next_arg;
        arg->value = strtok(NULL, " ");
    }

    return cmd;
}

void free_command(struct command *cmd) {
    struct arg *curr = cmd->args; // this one is surely initialized

    while (curr) {
        struct arg *next = curr->next_arg;
        free(curr);
        curr = next;
    }

    free(cmd);
}