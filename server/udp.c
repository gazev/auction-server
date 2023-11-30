#include <string.h>
#include <stdio.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "../utils/logging.h"
#include "../utils/validators.h"

#include "server.h"
#include "udp_command_table.h"
#include "udp_errors.h"
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
    LOG_DEBUG("%s:%d - Entered serve_udp_command", client->ipv4, client->port);
    char *cmd = strtok(request, " ");

    if (cmd == NULL) {
        LOG_DEBUG("%s:%d - NULL fn handler", client->ipv4, client->port);
        LOG_VERBOSE("%s:%d - No command supplied, error", client->ipv4, client->port);
        return -1;
    }

    if (strlen(cmd) != 3) {
        // printf("%ld\n", strlen(cmd));
        LOG_DEBUG("%s:%d - strlen(cmd) != 3, cmd = %s", client->ipv4, client->port, cmd);
        LOG_VERBOSE("%s:%d - Invalid command, ignoring", client->ipv4, client->port);
        return -1;
    }

    udp_handler_fn fn = get_udp_handler_fn(cmd);

    if (fn == NULL) {
        LOG_DEBUG("%s:%d - No command handler", client->ipv4, client->port)
        LOG_VERBOSE("%s:%d - Unknown command, ignoring", client->ipv4, client->port);
        return -1;
    }

    LOG_VERBOSE("%s:%d - Handling %s command", client->ipv4, client->port, cmd);

    int err = fn(request, client, response, response_len);
    // if the syntax of the request was incorrect or values were missing
    if (err) {
        LOG_VERBOSE("%s:%d - Badly formatted command", client->ipv4, client->port);
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
    LOG_DEBUG("%s:%d - Entered handle_login", client->ipv4, client->port);
    char *uid = strtok(NULL, " ");

    if (uid == NULL) {
        LOG_DEBUG("%s:%d - No UID supplied", client->ipv4, client->port);
        return ERR_LIN_BAD_VALUES;
    }

    if (!is_valid_uid(uid)) {
        LOG_DEBUG("%s:%d - Invalid UID", client->ipv4, client->port);
        return ERR_LIN_BAD_VALUES;
    }

    char *passwd = strtok(NULL, "\n");

    if (passwd == NULL) {
        LOG_DEBUG("%s:%d - No password supplied", client->ipv4, client->port);
        return ERR_LIN_BAD_VALUES;
    }

    if (!is_valid_passwd(passwd)) {
        LOG_DEBUG("%s:%d - Invalid passwd", client->ipv4, client->port);
        return ERR_LIN_BAD_VALUES;
    }

    // register user if it doesn't exist
    if (!exists_user(uid)) {
        if (register_user(uid, passwd) != 0) {
            LOG_VERBOSE("%s:%d - Error registering %s", client->ipv4, client->port, uid);
            sprintf(response, "RLI NOK\n");
            *response_len = 8;
            return 0;
        }

        LOG_VERBOSE("%s:%d - Registered user %s", client->ipv4, client->port, uid);
        sprintf(response, "RLI REG\n");
        *response_len = 8;
        return 0;
    }

    // authenticate user
    if (!is_authentic_user(uid, passwd)) {
        LOG_VERBOSE("%s:%d - User %s failed authentication", client->ipv4, client->port, uid);
        sprintf(response, "RLI NOK\n");
        *response_len = 8;
        return 0;
    }

    // login user
    if (log_in_user(uid) != 0) {
        LOG_VERBOSE("%s:%d - Failed logging user %s", client->ipv4, client->port, uid);
        // if an error occured during login
        sprintf(response, "RLI NOK\n");
        *response_len = 8;
        return 0;
    }

    LOG_VERBOSE("%s:%d - Logged in user %s", client->ipv4, client->port, uid);
    sprintf(response, "RLI OK\n"); 
    *response_len = 7;

    return 0;
}

int handle_logout(char *input, struct udp_client *client, char *response, size_t *response_len) {
    LOG_DEBUG("%s:%d - Entered handle_logout", client->ipv4, client->port);
    char *uid = strtok(NULL, " ");

    if (uid == NULL) {
        LOG_DEBUG("%s:%d - No UID supplied", client->ipv4, client->port);
        return ERR_LOU_BAD_VALUES;
    }

    if (!is_valid_uid(uid)) {
        LOG_DEBUG("%s:%d - Invalid UID", client->ipv4, client->port);
        return ERR_LOU_BAD_VALUES;
    }

    char *passwd = strtok(NULL, "\n");

    if (passwd == NULL) {
        LOG_DEBUG("%s:%d - No password supplied", client->ipv4, client->port);
        return ERR_LOU_BAD_VALUES;
    }

    if (!is_valid_passwd(passwd)) {
        LOG_DEBUG("%s:%d - Invalid password", client->ipv4, client->port);
        return ERR_LOU_BAD_VALUES;
    }

    // check if user exists
    if (!exists_user(uid)) {
        LOG_VERBOSE("%s:%d - User %s doesn't exist", client->ipv4, client->port, uid);
        sprintf(response, "RLO UNR\n");
        *response_len = 8;
        return 0;
    }

    // check if user is logged in
    if (!is_user_logged_in(uid)) {
        LOG_VERBOSE("%s:%d - User %s is not logged in", client->ipv4, client->port, uid);
        sprintf(response, "RLO NOK\n");
        *response_len = 8;
        return 0;
    }

    // check if user password and uid match
    if (!is_authentic_user(uid, passwd)) {
        LOG_VERBOSE("%s:%d - User %s failed authentication", client->ipv4, client->port, uid);
        sprintf(response, "RLO NOK\n");
        *response_len = 8;
        return 0;
    }

    // logout user
    if (log_out_user(uid) != 0) {
        LOG_VERBOSE("%s:%d - Failed logging out user %s", client->ipv4, client->port, uid);
        sprintf(response, "RLO NOK\n");
        *response_len = 8;
        return 0;
    }

    LOG_VERBOSE("%s:%d - Logged out user %s", client->ipv4, client->port, uid);
    sprintf(response, "RLO OK\n");
    *response_len = 8;

    return 0;
}

int handle_unregister(char *input, struct udp_client *client, char *response, size_t *response_len) {
    LOG_DEBUG("%s:%d - Entered handle_unregister", client->ipv4, client->port);
    char *uid = strtok(NULL, " ");

    if (uid == NULL) {
        LOG_DEBUG("%s:%d - No UID supplied", client->ipv4, client->port);
        return ERR_UNR_BAD_VALUES;
    }

    if (!is_valid_uid(uid)) {
        LOG_DEBUG("%s:%d - Invalid UID", client->ipv4, client->port);
        return ERR_UNR_BAD_VALUES;
    }

    char *passwd = strtok(NULL, "\n");

    if (passwd == NULL) {
        LOG_DEBUG("%s:%d - No password supplied", client->ipv4, client->port);
        return ERR_UNR_BAD_VALUES;
    }

    if (!is_valid_passwd(passwd)) {
        LOG_DEBUG("%s:%d - Invalid password", client->ipv4, client->port);
        return ERR_UNR_BAD_VALUES;
    }

    // check if user exists
    if (!exists_user(uid)) {
        LOG_VERBOSE("%s:%d - User %s doesn't exist", client->ipv4, client->port, uid);
        sprintf(response, "RUR UNR\n");
        *response_len = 8;
        return 0;
    }

    // check if user is logged in
    if (!is_user_logged_in(uid)) {
        LOG_VERBOSE("%s:%d - User %s isn't logged in", client->ipv4, client->port, uid);
        sprintf(response, "RUR NOK\n");
        *response_len = 8;
        return 0;
    }

    // authenticate user
    if (!is_authentic_user(uid, passwd)) {
        LOG_VERBOSE("%s:%d - User %s failed authentication", client->ipv4, client->port, uid);
        sprintf(response, "RUR NOK\n");
        *response_len = 8;
        return 0;
    }

    // unregister user
    if (unregister_user(uid) != 0) {
        LOG_VERBOSE("%s:%d - Failed unregistering user %s", client->ipv4, client->port, uid);
        // if an internal occurs during unregistration
        sprintf(response, "RUR NOK\n");
        *response_len = 8;
        return 0;
    }

    LOG_VERBOSE("%s:%d - Unregistered user %s", client->ipv4, client->port, uid);
    sprintf(response, "RUR OK\n");
    *response_len = 7;

    return 0;
}

int handle_my_auctions(char *input, struct udp_client *client, char *response, size_t *response_len) {
    LOG_DEBUG("entered");
    return 0;
}

int handle_my_bids(char *input, struct udp_client *client, char *response, size_t *response_len) {
    LOG_DEBUG("entered handle_my_bids");
    return 0;
}

int handle_list(char *input, struct udp_client *client, char *response, size_t *response_len) {
    LOG_DEBUG("entered handle_list");
    return 0;
}

int handle_show_record(char *input, struct udp_client *client, char *response, size_t *response_len) {
    LOG_DEBUG("entered handle_show_record");
    return 0;
}

