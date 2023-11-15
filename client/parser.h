#ifndef __PARSER_H__
#define __PARSER_H__

#define MAX_COMMAND_SIZE 1024

struct arg {
    char *value;
    struct arg *next_arg;
};

struct command {
    char *op; // command operation (e.g "login", "logout", etc)
    struct arg *args; // command arguments (e.g "uuid" -> "password")
};


struct command *parse_command(char *command);
void free_command(struct command *cmd);

#endif