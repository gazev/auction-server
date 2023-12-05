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
    {"q", handle_exit},
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
    {"clear", handle_clear},
    {"help", handle_help},
    {"h", handle_help},
};


// maps error codes (ints) of command handlers to the corresponding error message. 
// used by get_err_message(errcode)
const 
static 
struct error_mappings error_lookup_table[] = {
    {ERR_TIMEDOUT_UDP, "Timed out waiting for server UDP response\n"},
    {ERR_REQUESTING_UDP, "Failed sending UDP request to server\n"},
    {ERR_RECEIVING_UDP, "Failed reading UDP response from server\n"},
    {ERR_NULL_ARGS, "Missing arguments for command\n"},
    {ERR_INVALID_UID, "The UID must be numeric string of size 6\n"},
    {ERR_INVALID_PASSWD, "The password must be an alphanumeric string of size 8\n"},
    {ERR_INVALID_AID, "The AID must be a numeric string of size 3 (0 padded)\n"},
    {ERR_UNKNOWN_ANSWER, "Got an unknown response message from the server\n"},
    {ERR_FILE_DOESNT_EXIST, "Asset file doesn't exist\n"},
    {ERR_INVALID_ASSET_NAME, "Invalid asset name, must an alphanumeric with size less than 10\n"},
    {ERR_INVALID_ASSET_FNAME, "Invalid asset fname, might contain ilegal characters\n"},
    {ERR_INVALID_SV, "Invalid start value, must be a number with up to 6 digits\n"},
    {ERR_INVALID_TA, "Invalid time active, must be a number represented with up to 5 digits\n"},
    {ERR_INVALID_ASSET_FILE, "Couldn't reach specified asset file\n"},
    {ERR_TCP_CONN_TO_SERVER, "Failed establishing a connection to the server\n"},
    {ERR_TIMEOUT_TCP, "Timed out waiting for server TCP response\n"},
    {ERR_RECEIVING_TCP, "Failed reading TCP response from TCP server\n"},
    {ERR_REQUESTING_TCP, "Failed sending TCP request to server\n"},
    {ERR_NOT_LOGGED_IN, "Must be logged in to perform this operation\n"},
    {ERR_ALREADY_LOGGED_IN, "A user is already logged in\n"},
    {ERR_NOT_LOGGED_OUT, "Please logout before performing this operation\n"},
    {ERR_NO_COMMAND, ""},
    {ERR_INVALID_COMMAND, "Invalid command, type `help` for more information\n"},
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
