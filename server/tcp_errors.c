#include <stdlib.h>

// lookup table for strings that represent error codes returned from tcp_handler_fn 
// each error message is indexed by it's errcode, check tcp_errors.h
static
char *tcp_errors_table[] = {
    "", // return code 0 is not an error
    "ROA ERR\n", // OPA with bad args 
    "RCL ERR\n", // CLS with bad args 
    "RBD ERR\n", // VUD wutg vad args 
};

const
static
char tcp_error_table_entries = sizeof(tcp_errors_table) / sizeof (char *);

char *get_tcp_error_msg(int errcode) {
    if (errcode < 0 || errcode >= tcp_error_table_entries)
        return NULL;

    return tcp_errors_table[errcode];
}