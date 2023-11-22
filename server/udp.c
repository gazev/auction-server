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
int serve_udp_command(char *request, struct udp_client *client, char *response, size_t *response_len) {
    LOG_DEBUG("entered serve_udp_command");
    char *cmd = strtok(request, " ");

    if (cmd == NULL) {
        LOG_DEBUG("received no command");
        LOG_VERBOSE("%s - no command supplied", client->ipv4);
        return -1;
    }

    if (strlen(cmd) != 3) {
        // printf("%ld\n", strlen(cmd));
        LOG_DEBUG("strlen(cmd) != 3, cmd = %s", cmd)
        LOG_VERBOSE("%s - ignoring invalid command %s", client->ipv4, cmd);
        return -1;
    }

    udp_handler_fn fn = get_udp_handler_func(cmd);

    if (fn == NULL) {
        LOG_DEBUG("NULL fn handler")
        LOG_VERBOSE("%s - ignoring unkown command %s", client->ipv4, cmd);
        return -1;
    }

    LOG_VERBOSE("Handling %s request for %s", cmd, client->ipv4);

    int err = fn(request, client, response, response_len);
    // if the syntax of the request was incorrect or values were missing
    if (err) {
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
    LOG_DEBUG("entered handle_login");
    char *uid = strtok(NULL, " ");

    if (uid == NULL) {
        LOG_DEBUG("no UID supplied");
        return ERR_LIN_BAD_VALUES;
    }

    if (!is_valid_uid(uid)) {
        LOG_DEBUG("invalid UID");
        return ERR_LIN_BAD_VALUES;
    }

    char *passwd = strtok(NULL, "\n");

    if (passwd == NULL) {
        LOG_DEBUG("no password supplied")
        return ERR_LIN_BAD_VALUES;
    }

    if (!is_valid_passwd(passwd)) {
        LOG_DEBUG("invalid passwd");
        return ERR_LIN_BAD_VALUES;
    }

    LOG_VERBOSE("Handling login request for %s on %s", uid, client->ipv4);

    // register user if it doesn't exist
    if (!exists_user(uid)) {
        if (register_user(uid, passwd) != 0) {
            LOG_DEBUG("error registering %s", uid);
            sprintf(response, "RLI NOK\n");
            *response_len = 8;
            return 0;
        }

        sprintf(response, "RLI REG\n");
        *response_len = 8;
        return 0;
    }

    // authenticate user
    if (!is_authentic_user(uid, passwd)) {
        sprintf(response, "RLI NOK\n");
        *response_len = 8;
        return 0;
    }

    // login user
    if (log_in_user(uid) != 0) {
        LOG_DEBUG("error logging %s", uid);
        // if an error occured during login
        sprintf(response, "RLI NOK\n");
        *response_len = 8;
        return 0;
    }

    sprintf(response, "RLI OK\n"); 
    *response_len = 7;

    return 0;
}

int handle_logout(char *input, struct udp_client *client, char *response, size_t *response_len) {
    LOG_DEBUG("entered handle_logout");
    char *uid = strtok(NULL, " ");

    if (uid == NULL) {
        return ERR_LOU_BAD_VALUES;
    }

    if (!is_valid_uid(uid)) {
        return ERR_LOU_BAD_VALUES;
    }

    char *passwd = strtok(NULL, "\n");

    if (passwd == NULL) {
        return ERR_LOU_BAD_VALUES;
    }

    if (!is_valid_passwd(passwd)) {
        return ERR_LOU_BAD_VALUES;
    }

    LOG_VERBOSE("Handling logout request for %s on %s", uid, client->ipv4);

    // check if user exists
    if (!exists_user(uid)) {
        sprintf(response, "RLO UNR\n");
        *response_len = 8;
        return 0;
    }

    // check if user is logged in
    if (!is_user_logged_in(uid)) {
        sprintf(response, "RLO NOK\n");
        *response_len = 8;
        return 0;
    }

    // check if user password and uid match
    if (!is_authentic_user(uid, passwd)) {
        sprintf(response, "RLO NOK\n");
        *response_len = 8;
        return 0;
    }

    // logout user
    if (log_out_user(uid) != 0) {
        LOG_DEBUG("failed logging out user");
        sprintf(response, "RLO NOK\n");
        *response_len = 8;
        return 0;
    }

    sprintf(response, "RLO OK\n");
    *response_len = 8;

    return 0;
}

int handle_unregister(char *input, struct udp_client *client, char *response, size_t *response_len) {
    LOG_DEBUG("entered handle_unregister");
    char *uid = strtok(NULL, " ");

    if (uid == NULL) {
        return ERR_UNR_BAD_VALUES;
    }

    if (!is_valid_uid(uid)) {
        return ERR_UNR_BAD_VALUES;
    }

    char *passwd = strtok(NULL, "\n");

    if (passwd == NULL) {
        return ERR_UNR_BAD_VALUES;
    }

    if (!is_valid_passwd(passwd)) {
        return ERR_UNR_BAD_VALUES;
    }

    LOG_VERBOSE("Handling logout request for %s on %s", uid, client->ipv4);

    // check if user exists
    if (!exists_user(uid)) {
        sprintf(response, "RUR UNR\n");
        *response_len = 8;
        return 0;
    }

    // check if user is logged in
    if (!is_user_logged_in(uid)) {
        sprintf(response, "RUR NOK\n");
        *response_len = 8;
        return 0;
    }

    // authenticate user
    if (!is_authentic_user(uid, passwd)) {
        sprintf(response, "RUR NOK\n");
        *response_len = 8;
        return 0;
    }

    // unregister user
    if (unregister_user(uid) != 0) {
        // if an internal occurs during unregistration
        sprintf(response, "RUR NOK\n");
        *response_len = 8;
        return 0;
    }

    sprintf(response, "RUR OK\n");
    *response_len = 7;

    return 0;
}

int handle_exit(char *input, struct udp_client *client, char *response, size_t *response_len) {
    LOG_DEBUG("entered handle_exit");
    return 0;
}

int handle_open(char *input, struct udp_client *client, char *response, size_t *response_len) {
    LOG_DEBUG("entered handle_open");
    return 0;
}

int handle_close(char *input, struct udp_client *client, char *response, size_t *respnose_len) {
    LOG_DEBUG("entered handle_close");
    return 0;
}

int handle_my_auctions(char *input, struct udp_client *client, char *response, size_t *response_len) {
    LOG_DEBUG("entered");
    return 0;
}

