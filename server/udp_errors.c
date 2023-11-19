#include <stdlib.h>

// lookup table for strings that represent error codes returned from udp handler funcs
// each error message is indexed by it's errcode, check udp_errors.h to understand better
const
static
char *errors_table[] = {
    "", // return code 0 is not an error
    "RLI ERR\n", // login with bad UID and passwd
    "RLO ERR\n", // logout with bad UID and passwd
    "RUR ERR\n", // unregister with bad UID and passwd
    "RMA ERR\n", // my auctions with bad UID
    "RMB ERR\n", // my bids with bad UID
    "RRC ERR\n", // show record with bad AID
};
// this was done like this because it grants extensibility, for example, if we want to
// communicate differente errors, say, wrong password and wrong uid to login we could simply
// define two different errorcodes here and it would just work with little changes in
// handle_login

const
static
char error_table_entries = sizeof(errors_table) / sizeof (char *);

const char *get_udp_error_msg(int errcode) {
    if (errcode < 0 || errcode >= error_table_entries)
        return NULL;

    return errors_table[errcode];
}