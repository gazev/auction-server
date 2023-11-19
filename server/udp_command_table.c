#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../utils/logging.h"
#include "udp.h"

#include "udp_command_table.h"

struct command_mappings {
    const char cmd_op[3];
    udp_handler_fn func;
};

// maps the command string to the their respective function handler 
const 
static 
struct command_mappings udp_command_table[] = {
    {"LIN", handle_login},
    {"LOU", handle_logout},
    {"UNR", handle_unregister},
    {"LMA", handle_exit},
    {"LMB", handle_open},
    {"LST", handle_close},
    {"SRC", handle_my_auctions},
};


const 
static 
int udp_command_table_entries = sizeof(udp_command_table) / sizeof(struct command_mappings);

/**
Get function handler for command
*/
udp_handler_fn get_udp_handler_func(char *cmd) {
    LOG_DEBUG("entered get_handler_func");
    for (int i = 0; i < udp_command_table_entries; ++i) {
        if (strcmp(cmd, udp_command_table[i].cmd_op) == 0) {
            return udp_command_table[i].func;
        }
    }

    return NULL;
}