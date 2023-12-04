#include <string.h>
#include <stdio.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../utils/logging.h"
#include "../utils/validators.h"

#include "server.h"
#include "udp_command_table.h"
#include "database.h"

#include "udp.h"

/**
* Hanle a UDP protocol command.
* Returns 0 if command was sucessfully instructed by the program and -1 if the
* command wasn't recognized.
* 
* If 0 is returned, the server response shall be written to response and the response_len
* shall be set to indicate the response's size
*/
int handle_udp_command(char *request, struct udp_client *client, char *response, size_t *response_len) {
    LOG_DEBUG("%s:%d - [UDP] Serving client", client->ipv4, client->port);
    char *cmd = strtok(request, " ");

    if (cmd == NULL) {
        LOG_VERBOSE("%s:%d - [UDP] No command supplied, error", client->ipv4, client->port);
        return -1;
    }

    if (strlen(cmd) != 3) {
        // printf("%ld\n", strlen(cmd));
        LOG_DEBUG("%s:%d - [UDP] Not enough characters for command token", client->ipv4, client->port);
        LOG_VERBOSE("%s:%d - [UDP] Invalid command, ignoring", client->ipv4, client->port);
        return -1;
    }

    udp_handler_fn fn = get_udp_handler_fn(cmd);

    if (fn == NULL) {
        LOG_VERBOSE("%s:%d - [UDP] Unknown command, ignoring", client->ipv4, client->port);
        return -1;
    }

    LOG_VERBOSE("%s:%d - [UDP] Handling %s command", client->ipv4, client->port, cmd);

    int err = fn(request, client, response, response_len);
    // if the syntax of the request was incorrect or values were missing
    if (err) {
        LOG_VERBOSE("%s:%d - [UDP] Badly formatted command", client->ipv4, client->port);
        char *error_msg = get_udp_error_msg(err);
        sprintf(response, "%s", error_msg);
        *response_len = strlen(error_msg);
    }

    return 0;
}


/**
* Performs a login request.
* If an error occurs the corresponding error value is returned. If successful
* 0 is returned
**/
int handle_login(char *input, struct udp_client *client, char *response, size_t *response_len) {
    LOG_DEBUG("%s:%d - [LIN] Entered handler", client->ipv4, client->port);
    char *uid = strtok(NULL, " ");

    if (uid == NULL) {
        LOG_VERBOSE("%s:%d - [LIN] No UID supplied", client->ipv4, client->port);
        return ERR_LIN_BAD_VALUES;
    }

    if (!is_valid_uid(uid)) {
        LOG_VERBOSE("%s:%d - [LIN] Invalid UID", client->ipv4, client->port);
        return ERR_LIN_BAD_VALUES;
    }

    char *passwd = strtok(NULL, " ");

    if (passwd == NULL) {
        LOG_VERBOSE("%s:%d - [LIN] No password supplied", client->ipv4, client->port);
        return ERR_LIN_BAD_VALUES;
    }

    if (!is_valid_passwd(passwd)) {
        LOG_VERBOSE("%s:%d - [LIN] Invalid password", client->ipv4, client->port);
        return ERR_LIN_BAD_VALUES;
    }

    // register user if it doesn't exist
    if (!exists_user(uid)) {
        if (register_user(uid, passwd) != 0) {
            LOG_VERBOSE("%s:%d - [LIN] Error registering user %s", client->ipv4, client->port, uid);
            sprintf(response, "RLI NOK\n");
            *response_len = 8;
            return 0;
        }

        LOG_VERBOSE("%s:%d - [LIN] Registered user %s", client->ipv4, client->port, uid);
        sprintf(response, "RLI REG\n");
        *response_len = 8;
        return 0;
    }

    // authenticate user
    if (!is_authentic_user(uid, passwd)) {
        LOG_VERBOSE("%s:%d - [LIN] User %s failed authentication", client->ipv4, client->port, uid);
        sprintf(response, "RLI NOK\n");
        *response_len = 8;
        return 0;
    }

    // login user
    if (log_in_user(uid) != 0) {
        LOG_VERBOSE("%s:%d - [LIN] Failed logging user %s", client->ipv4, client->port, uid);
        // if an error occured during login
        sprintf(response, "RLI NOK\n");
        *response_len = 8;
        return 0;
    }

    LOG_VERBOSE("%s:%d - [LIN] Logged in user %s", client->ipv4, client->port, uid);
    sprintf(response, "RLI OK\n"); 
    *response_len = 7;

    return 0;
}

int handle_logout(char *input, struct udp_client *client, char *response, size_t *response_len) {
    LOG_DEBUG("%s:%d - [LOU] Entered handler", client->ipv4, client->port);
    char *uid = strtok(NULL, " ");

    if (uid == NULL) {
        LOG_VERBOSE("%s:%d - [LOU] No UID supplied", client->ipv4, client->port);
        return ERR_LOU_BAD_VALUES;
    }

    if (!is_valid_uid(uid)) {
        LOG_VERBOSE("%s:%d - [LOU] Invalid UID", client->ipv4, client->port);
        return ERR_LOU_BAD_VALUES;
    }

    char *passwd = strtok(NULL, " ");

    if (passwd == NULL) {
        LOG_VERBOSE("%s:%d - [LOU] No password supplied", client->ipv4, client->port);
        return ERR_LOU_BAD_VALUES;
    }

    if (!is_valid_passwd(passwd)) {
        LOG_VERBOSE("%s:%d - [LOU] Invalid password", client->ipv4, client->port);
        return ERR_LOU_BAD_VALUES;
    }

    // check if user exists
    if (!exists_user(uid)) {
        LOG_VERBOSE("%s:%d - [LOU] User %s doesn't exist", client->ipv4, client->port, uid);
        sprintf(response, "RLO UNR\n");
        *response_len = 8;
        return 0;
    }

    // check if user is logged in
    if (!is_user_logged_in(uid)) {
        LOG_VERBOSE("%s:%d - [LOU] User %s is not logged in", client->ipv4, client->port, uid);
        sprintf(response, "RLO NOK\n");
        *response_len = 8;
        return 0;
    }

    // check if user password and uid match
    if (!is_authentic_user(uid, passwd)) {
        LOG_VERBOSE("%s:%d - [LOU] User %s failed authentication", client->ipv4, client->port, uid);
        sprintf(response, "RLO NOK\n");
        *response_len = 8;
        return 0;
    }

    // logout user
    if (log_out_user(uid) != 0) {
        LOG_VERBOSE("%s:%d - [LOU] Failed logging out user %s", client->ipv4, client->port, uid);
        sprintf(response, "RLO NOK\n");
        *response_len = 8;
        return 0;
    }

    LOG_VERBOSE("%s:%d - [LOU] Logged out user %s", client->ipv4, client->port, uid);
    sprintf(response, "RLO OK\n");
    *response_len = 8;

    return 0;
}

int handle_unregister(char *input, struct udp_client *client, char *response, size_t *response_len) {
    LOG_DEBUG("%s:%d - [UNR] Entered handler", client->ipv4, client->port);
    char *uid = strtok(NULL, " ");

    if (uid == NULL) {
        LOG_VERBOSE("%s:%d - [UNR] No UID supplied", client->ipv4, client->port);
        return ERR_UNR_BAD_VALUES;
    }

    if (!is_valid_uid(uid)) {
        LOG_VERBOSE("%s:%d - [UNR] Invalid UID", client->ipv4, client->port);
        return ERR_UNR_BAD_VALUES;
    }

    char *passwd = strtok(NULL, " ");

    if (passwd == NULL) {
        LOG_VERBOSE("%s:%d - [UNR] No password supplied", client->ipv4, client->port);
        return ERR_UNR_BAD_VALUES;
    }

    if (!is_valid_passwd(passwd)) {
        LOG_VERBOSE("%s:%d - [UNR] Invalid password", client->ipv4, client->port);
        return ERR_UNR_BAD_VALUES;
    }

    // check if user exists
    if (!exists_user(uid)) {
        LOG_VERBOSE("%s:%d - [UNR] User %s doesn't exist", client->ipv4, client->port, uid);
        sprintf(response, "RUR UNR\n");
        *response_len = 8;
        return 0;
    }

    // check if user is logged in
    if (!is_user_logged_in(uid)) {
        LOG_VERBOSE("%s:%d - [UNR] User %s isn't logged in", client->ipv4, client->port, uid);
        sprintf(response, "RUR NOK\n");
        *response_len = 8;
        return 0;
    }

    // authenticate user
    if (!is_authentic_user(uid, passwd)) {
        LOG_VERBOSE("%s:%d - [UNR] User %s failed authentication", client->ipv4, client->port, uid);
        sprintf(response, "RUR NOK\n");
        *response_len = 8;
        return 0;
    }

    // unregister user
    if (unregister_user(uid) != 0) {
        LOG_VERBOSE("%s:%d - [UNR] Failed unregistering user %s", client->ipv4, client->port, uid);
        // if an internal occurs during unregistration
        sprintf(response, "RUR NOK\n");
        *response_len = 8;
        return 0;
    }

    LOG_VERBOSE("%s:%d - [UNR] Unregistered user %s", client->ipv4, client->port, uid);
    sprintf(response, "RUR OK\n");
    *response_len = 7;

    return 0;
}

int handle_my_auctions(char *input, struct udp_client *client, char *response, size_t *response_len) {
    LOG_DEBUG("%s:%d - [LMA] Entered handler", client->ipv4, client->port);
    update_database();
    char *uid = strtok(NULL, "\n");

    if (uid == NULL) {
        LOG_VERBOSE("%s:%d - [LMA] No UID supplied", client->ipv4, client->port);
        return ERR_LMA_BAD_VALUES;
    }

    if (!is_valid_uid(uid)) {
        LOG_VERBOSE("%s:%d - [LMA] Invalid UID", client->ipv4, client->port);
        return ERR_LMA_BAD_VALUES;
    }

    // check if user exists
    if (!exists_user(uid) || !is_user_logged_in(uid)) {
        LOG_VERBOSE("%s:%d - [LMA] User %s doesn't exist or isn't logged in", client->ipv4, client->port, uid);
        sprintf(response, "RMA NLG\n");
        *response_len = 8;
        return 0;
    }

    /**
    * Start creating my auctions response
    */
    sprintf(response, "RMA OK");
    *response_len = 6;
    // write response
    int written = get_user_auctions(uid, response);

    // error reading user auctions
    if (written < 0) {
        LOG_VERBOSE("%s:%d - [LMA] Failed retrieving user %s auctions", client->ipv4, client->port, uid);
        sprintf(response, "RMA NOK\n");
        *response_len = 8;
        return 0;
    }

    // user has no auctions
    if (written == 1) {
        LOG_VERBOSE("%s:%d - [LMA] No aucions for user %s", client->ipv4, client->port, uid);
        sprintf(response, "RMA NOK\n");
        *response_len = 8;
        return 0;
    }

    *response_len += written;

    LOG_VERBOSE("%s:%d - [LMA] Sent user %s auction list to client", client->ipv4, client->port, uid);

    return 0;
}

int handle_my_bids(char *input, struct udp_client *client, char *response, size_t *response_len) {
    LOG_DEBUG("entered handle_my_bids");
    update_database();
    return 0;
}

int handle_list(char *input, struct udp_client *client, char *response, size_t *response_len) {
    LOG_DEBUG("%s:%d - [LST] Entered handler", client->ipv4, client->port);
    update_database();

    // get auctions list
    sprintf(response, "RLS OK");
    *response_len = 6;
    int written = get_auctions_list(response);

    if (written < 0) {
        LOG_VERBOSE("%s:%d - [LST] Failed retrieving auctions list", client->ipv4, client->port);
        sprintf(response, "RLS NOK\n");
        *response_len = 8;
        return 0;
    }

    if (written == 1) {
        LOG_VERBOSE("%s:%d - [LST] No aucions in server", client->ipv4, client->port);
        sprintf(response, "RLS NOK\n");
        *response_len = 8;
        return 0;
    }

    *response_len += written;

    LOG_VERBOSE("%s:%d - [LST] Sent auction list to client", client->ipv4, client->port);

    return 0;
}

int handle_show_record(char *input, struct udp_client *client, char *response, size_t *response_len) {
    LOG_DEBUG("%s:%d - [SRC] Entered handler", client->ipv4, client->port);
    char *aid = strtok(input, " "); 

    if (aid == NULL) {
        LOG_VERBOSE("%s:%d - [SRC] No UID supplied", client->ipv4, client->port);
        return ERR_SRC_BAD_VALUES; 
    }

    if (!is_valid_aid(aid)) {
        LOG_VERBOSE("%s:%d - [SRC] Invalid UID", client->ipv4, client->port);
        return ERR_SRC_BAD_VALUES;
    }

    if (!exists_auction(aid)) {
        LOG_VERBOSE("%s:%d - [SRC] Auction %s doesn't exist", client->ipv4, client->port, aid);
        sprintf(response, "RRC NOK\n");
        *response_len = 8;
        return 0;
    }

    return 0;
}

