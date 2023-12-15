#include <stdio.h>
#include <string.h>

#include "../utils/logging.h"
#include "../utils/config.h"
#include "../utils/constants.h"

#include "tcp.h"
#include "tcp_command_table.h"

struct tcp_command_mappings {
    const char cmd_op[5];
    tcp_handler_fn func;
};

/**
* TCP Command table
*/
static 
const 
struct tcp_command_mappings tcp_command_table[] = {
    {"OPA ", handle_open},
    {"CLS ", handle_close},
    {"SAS ", handle_show_asset},
    {"BID ", handle_bid},
};


// lookup table for strings that represent error codes returned from tcp_handler_fn 
// each error message is indexed by it's errcode, check tcp_errors.h
static
char *tcp_errors_table[] = {
    "", // return code 0 is not an error
    "ROA ERR\n", // OPA with bad args 
    "RCL ERR\n", // CLS with bad args 
    "RSA ERR\n", // SAS with bad args 
    "RBD ERR\n", // BID with bad args 
};

static
const
int opa_args_len[] = {
    ASSET_NAME_LEN,
    START_VALUE_LEN,
    TIME_ACTIVE_LEN,
    FNAME_LEN,
    FSIZE_STR_LEN,
};


static 
const 
int tcp_command_table_entries = sizeof(tcp_command_table) / sizeof(struct tcp_command_mappings);

const
static
char tcp_error_table_entries = sizeof(tcp_errors_table) / sizeof (char *);

/**
Get function handler for command
*/
tcp_handler_fn get_tcp_handler_fn(char *cmd) {
    for (int i = 0; i < tcp_command_table_entries; ++i) {
        if (strcmp(cmd, tcp_command_table[i].cmd_op) == 0) {
            return tcp_command_table[i].func;
        }
    }

    return NULL;
}


int get_opa_arg_len(int argno) {
    if (argno < 0 || argno > 5) {
        return 0;
    }

    return opa_args_len[argno];
}


char *get_tcp_error_msg(int errcode) {
    if (errcode < 0 || errcode >= tcp_error_table_entries)
        return NULL;

    return tcp_errors_table[errcode];
}