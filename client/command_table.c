#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdio.h>

#include "../utils/logging.h"

#include "command_table.h"
#include "handlers.h"

struct command_mappings {
    const char *cmd_op;
    handler_func func;
};

struct error_mappings {
    int errcode;
    char *err_msg;
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


// maps error codes (ints) of command handlers to the corresponding error message. 
// used by get_err_message(errcode)
const 
static 
struct error_mappings error_lookup_table[] = {
    {ERR_TIMEDOUT_UDP, "Timed out waiting for server"},
    {ERR_REQUESTING_UDP, "Failed sending request to server"},
    {ERR_RECEIVING_UDP, "Failed reading response from server"},
    {ERR_NULL_ARGS, "Missing arguments for command"},
    {ERR_INVALID_UID, "The UID must be numeric string of size 6"},
    {ERR_INVALID_PASSWD, "The password must be an alphanumeric string of size 8"},
    {ERR_INVALID_AID, "The AID must be a numeric string of size 3 (0 padded)"},
    {ERR_UNKNOWN_ANSWER, "Got an unknown response message from the server"},
};

const 
static 
int error_lookup_entries = sizeof(error_lookup_table) / sizeof(struct error_mappings); 

const 
static 
int command_table_entries = sizeof(command_table) / sizeof(struct command_mappings);

char *get_error_msg(int errcode) {

    if (errcode < 0 || errcode > error_lookup_entries) {
        LOG_ERROR("Don't provide errcode which are not returned");
        return NULL;
    }

    return error_lookup_table[errcode - 1].err_msg;
}

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
