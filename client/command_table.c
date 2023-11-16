#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "../utils/logging.h"

#include "command_table.h"
#include "parser.h"
#include "handlers.h"

struct command_mappings {
    const char *cmd_op;
    handler_func func;
};

// maps the command string to the their respective function handler 
const 
static 
struct command_mappings command_table[] = {
    {"login", handle_login},
    {"logout", handle_logout},
    {"unregister", handle_unregister},
    {"exit", handle_exit},
    {"open", handle_open},
    {"close", handle_close},
    {"myauctions", handle_my_auctions},
    {"ma", handle_my_auctions},
    {"mybids", handle_my_bids},
    {"mb", handle_my_bids},
    {"list", handle_list},
    {"l", handle_list},
    {"show_asset", handle_show_asset},
    {"sa", handle_show_asset},
    {"bid", handle_bid},
    {"b", handle_bid},
    {"show_record", handle_show_record},
    {"sr", handle_show_record},
};


const 
static 
int command_table_entries = sizeof(command_table) / sizeof(struct command_mappings);

/**
Get function handler for command
*/
handler_func get_handler_func(char *cmd) {
    for (int i = 0; i < command_table_entries; ++i) {
        if (strcmp(cmd, command_table[i].cmd_op) == 0) {
            return command_table[i].func;
        }
    }

    return NULL;
}
