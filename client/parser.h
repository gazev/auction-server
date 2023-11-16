#ifndef __PARSER_H__
#define __PARSER_H__

#define MAX_COMMAND_SIZE 1025 

struct arg {
    char *value;
    struct arg *next_arg;
};

struct command {
    char *op;
    struct arg *args;
};

struct command *parse_command(char *command);
void free_command(struct command *);

#endif