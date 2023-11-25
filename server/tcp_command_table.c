#include <stdio.h>
#include <string.h>

#include "../utils/logging.h"
#include "../utils/config.h"

#include "tcp.h"
#include "tcp_command_table.h"

struct tcp_command_mappings {
    const char cmd_op[3];
    tcp_handler_fn func;
};

// maps the command string to the their respective function handler 
const 
static 
struct tcp_command_mappings tcp_command_table[] = {
    {"OPA", handle_open},
    {"CLS", handle_close},
    {"SAS", handle_show_asset},
    {"BID", handle_bid},
};

const 
static 
int tcp_command_table_entries = sizeof(tcp_command_table) / sizeof(struct tcp_command_mappings);

const
static int opa_args_len[] = {
    ASSET_NAME_LEN,
    START_VALUE_LEN,
    TIME_ACTIVE_LEN,
    FNAME_LEN,
    FSIZE_STR_LEN,
};

/**
Get function handler for command
*/
tcp_handler_fn get_tcp_handler_fn(char *cmd) {
    LOG_DEBUG("entered get_handler_func");
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