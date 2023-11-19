#include <stdlib.h>
#include <stdio.h>

#include "../utils/logging.h"
#include "command_error.h"

struct error_mappings {
    int errcode;
    char *err_msg;
};


// maps error codes (ints) of command handlers to the corresponding error message. 
// used by get_err_message(errcode)
const 
static 
struct error_mappings error_lookup_table[] = {
    {ERROR_LOGIN, "Failed login"},
    {ERROR_LOGOUT, "Failed logout"},
    // put other errors here if necessary
};

const 
static 
int error_lookup_entries = sizeof(error_lookup_table) / sizeof(struct error_mappings); 

char *get_error_msg(int errcode) {
    if (errcode < 0 || errcode > error_lookup_entries) {
        LOG_DEBUG("Don't provide errcode which are not returned");
        return NULL;
    }

    return error_lookup_table[errcode - 1].err_msg;
}