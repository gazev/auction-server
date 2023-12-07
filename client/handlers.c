#include <stdio.h>
#include <sys/sendfile.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../utils/logging.h"
#include "../utils/validators.h"
#include "../utils/config.h"

#include "command_table.h"
#include "handlers.h"
#include "client.h"

/**
Handles command passed as argument.
Returns 0 if command is successfully executed and -1 if an error occurs
*/
int handle_cmd(char *user_input, struct client_state *client, char *response) {
    char *cmd = strtok(user_input, " "); 

    // no command, we just ignore and continue
    if (cmd == NULL) {
        return ERR_NO_COMMAND;
    }

    handler_func fn = get_handler_func(cmd);

    if (fn == NULL) {
        sprintf(response, "Unknown command %s", cmd);
        return ERR_INVALID_COMMAND;
    }
    

    return fn(user_input + strlen(cmd) + 1, client, response);
}

int handle_help(char *input, struct client_state *c, char *response) {
    char *help_msg = 
            "Available commands:\n\n"

            "help or h:        Print this message\n"
            "format or f:      Print formats of various command's messages\n"
            "clear:            Clear terminal window\n"
            "login:            Login to the AS\n"
            "open:             Open a new auction\n"
            "close:            Close an ongoing auction\n"
            "list or l:        List all available auctions\n"
            "myauctions or ma: List a user auctions\n"
            "mybids or mb:     List a user bids\n"
            "showasset or sa:  Show asset of an auction\n"
            "showrecord or sr: Show information of an auction\n"
            "bid or b:         Bid on an auction\n"
            "logout:           Logout the currently logged in user\n"
            "unregister:       Unregister the currently logged in user\n"
            "exit or q:        Exit the program\n";
 
    sprintf(response, "%s", help_msg);
    
    return 0;
}

int handle_clear(char *input, struct client_state *c, char *response) {
    strcpy(response, "\033[2J\033[H");
    return 0;
}

int handle_format(char *input, struct client_state *c, char *response) {
    char *msg_format =
        "Messages format:\n\n"
        "list\n"
        "  Format:\n"
        "  |{AID - Status}| (Up to 6 auctions per line)\n\n"

        "  Status Legend:\n"
        "  A - Active, C - Closed\n\n"

        "myauctions\n"
        "  Format:\n"
        "  |{AID - Status}| (Up to 6 auctions per line)\n\n"

        "  Status Legend:\n"
        "  A - Active, C - Closed\n\n"

        "showrecord\n"
        "  Format:\n"
        "  {Auction Information}\n"
        "  {Bids History}\n\n"

        "  Bids History Format:\n"
        "  |{Bidder UID - Bid Value - Bid Date - Bid Sec Time}| (Up to 2 per line)\n";

    strcpy(response, msg_format);
    return 0;
}