#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../utils/logging.h"

#include "udp.h"
#include "udp_command_table.h"

#define ERR_LIN_BAD_VALUES 1
#define ERR_LOU_BAD_VALUES 2
#define ERR_UNR_BAD_VALUES 3 
#define ERR_LMA_BAD_VALUES 4
#define ERR_MY_BIDS_BAD_UID 5
#define ERR_SHOW_ERROR_BAD_AID 6

struct udp_command_mappings {
    const char cmd_op[3];
    udp_handler_fn func;
};

/**
* UDP command table
*/
static 
const 
struct udp_command_mappings udp_command_table[] = {
    {"LIN", handle_login},
    {"LOU", handle_logout},
    {"UNR", handle_unregister},
    {"LMA", handle_my_auctions},
    {"LMB", handle_my_bids},
    {"LST", handle_list},
    {"SRC", handle_show_record},
};

// lookup table for strings that represent error codes returned from udp handler funcs
// each error message is indexed by it's errcode, check udp_errors.h to understand better
static
char *udp_errors_table[] = {
    "", // return code 0 is not an error
    "RLI ERR\n", // login with bad UID or passwd
    "RLO ERR\n", // logout with bad UID or passwd
    "RUR ERR\n", // unregister with bad UID or passwd
    "RMA ERR\n", // my auctions with bad UID or passwd
    "RMB ERR\n", // my bids with bad UID
    "RRC ERR\n", // show record with bad AID
};

static
const
char udp_error_table_entries = sizeof(udp_errors_table) / sizeof (char *);

static 
const 
int udp_command_table_entries = sizeof(udp_command_table) / sizeof(struct udp_command_mappings);

/**
Get function handler for command
*/
udp_handler_fn get_udp_handler_fn(char *cmd) {
    LOG_DEBUG("entered get_handler_func");
    for (int i = 0; i < udp_command_table_entries; ++i) {
        if (strcmp(cmd, udp_command_table[i].cmd_op) == 0) {
            return udp_command_table[i].func;
        }
    }

    return NULL;
}

char *get_udp_error_msg(int errcode) {
    if (errcode < 0 || errcode >= udp_error_table_entries)
        return NULL;

    return udp_errors_table[errcode];
}

