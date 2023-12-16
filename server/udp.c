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
    // read the command
    char cmd[5] = {0};
    strncpy(cmd, request, 4);

    udp_handler_fn fn = get_udp_handler_fn(cmd);
    if (fn == NULL) {
        LOG_VERBOSE("%s:%d - [UDP] Unknown command, ignoring", client->ipv4, client->port);
        return -1;
    }

    LOG_VERBOSE("%s:%d - [UDP] Handling %.3s command", client->ipv4, client->port, cmd);

    int err = fn(request + strlen(cmd), client, response, response_len);
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
int handle_login(char input[], struct udp_client *client, char *response, size_t *response_len) {
    LOG_DEBUG("%s:%d - [LIN] Entered handler", client->ipv4, client->port);
    /** 
    * Validate message parameters
    */
    char *uid, *passwd;

    uid = strtok(input, " ");
    if (uid == NULL) {
        LOG_VERBOSE("%s:%d - [LIN] No UID supplied", client->ipv4, client->port);
        return ERR_LIN;
    }

    if (!is_valid_uid(uid)) {
        LOG_VERBOSE("%s:%d - [LIN] Invalid UID", client->ipv4, client->port);
        return ERR_LIN;
    }

    // NOTE this makes the server permissive, that is, if we receive a LIN UID PASSWD\0
    // we also accepted, it's not the end of the world
    passwd = strtok(NULL, "\n");
    if (passwd == NULL) {
        LOG_VERBOSE("%s:%d - [LIN] No password supplied", client->ipv4, client->port);
        return ERR_LIN;
    }

    if (!is_valid_passwd(passwd)) {
        LOG_VERBOSE("%s:%d - [LIN] Invalid password", client->ipv4, client->port);
        return ERR_LIN;
    }

    // register user if it doesn't exist
    if (!exists_user(uid)) {
        if (register_user(uid, passwd) != 0)
            return ERR_LIN;

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
        LOG_VERBOSE("%s:%d - [LIN] Internal error logging in user %s", client->ipv4, client->port, uid);
        return ERR_LIN;
    }

    LOG_VERBOSE("%s:%d - [LIN] Logged in user %s", client->ipv4, client->port, uid);
    sprintf(response, "RLI OK\n"); 
    *response_len = 7;

    return 0;
}


int handle_logout(char *input, struct udp_client *client, char *response, size_t *response_len) {
    LOG_DEBUG("%s:%d - [LOU] Entered handler", client->ipv4, client->port);
    /** 
    * Validate message parameters
    */
    char *uid, *passwd;

    uid = strtok(input, " ");
    if (uid == NULL) {
        LOG_VERBOSE("%s:%d - [LOU] No UID supplied", client->ipv4, client->port);
        return ERR_LOU;
    }

    if (!is_valid_uid(uid)) {
        LOG_VERBOSE("%s:%d - [LOU] Invalid UID", client->ipv4, client->port);
        return ERR_LOU;
    }

    passwd = strtok(NULL, "\n");
    if (passwd == NULL) {
        LOG_VERBOSE("%s:%d - [LOU] No password supplied", client->ipv4, client->port);
        return ERR_LOU;
    }

    if (!is_valid_passwd(passwd)) {
        LOG_VERBOSE("%s:%d - [LOU] Invalid password", client->ipv4, client->port);
        return ERR_LOU;
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
        LOG_VERBOSE("%s:%d - [LOU] Internal error logging out user %s", client->ipv4, client->port, uid);
        return ERR_LOU;
    }

    LOG_VERBOSE("%s:%d - [LOU] Logged out user %s", client->ipv4, client->port, uid);
    sprintf(response, "RLO OK\n");
    *response_len = 7;

    return 0;
}


int handle_unregister(char *input, struct udp_client *client, char *response, size_t *response_len) {
    LOG_DEBUG("%s:%d - [UNR] Entered handler", client->ipv4, client->port);

    /** 
    * Validate message parameters
    */
    char *uid, *passwd;
    uid = strtok(input, " ");
    if (uid == NULL) {
        LOG_VERBOSE("%s:%d - [UNR] No UID supplied", client->ipv4, client->port);
        return ERR_UNR;
    }

    if (!is_valid_uid(uid)) {
        LOG_VERBOSE("%s:%d - [UNR] Invalid UID", client->ipv4, client->port);
        return ERR_UNR;
    }

    passwd = strtok(NULL, "\n");
    if (passwd == NULL) {
        LOG_VERBOSE("%s:%d - [UNR] No password supplied", client->ipv4, client->port);
        return ERR_UNR;
    }

    if (!is_valid_passwd(passwd)) {
        LOG_VERBOSE("%s:%d - [UNR] Invalid password", client->ipv4, client->port);
        return ERR_UNR;
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
        LOG_VERBOSE("%s:%d - [UNR] Internal error unregistering user %s", client->ipv4, client->port, uid);
        return ERR_UNR;
    }

    LOG_VERBOSE("%s:%d - [UNR] Unregistered user %s", client->ipv4, client->port, uid);
    sprintf(response, "RUR OK\n");
    *response_len = 7;

    return 0;
}


int handle_my_auctions(char *input, struct udp_client *client, char *response, size_t *response_len) {
    LOG_DEBUG("%s:%d - [LMA] Entered handler", client->ipv4, client->port);
    /**
    * validate command arguments 
    */
    char *uid = strtok(input, "\n");
    if (uid == NULL) {
        LOG_VERBOSE("%s:%d - [LMA] No UID supplied", client->ipv4, client->port);
        return ERR_LMA;
    }

    if (!is_valid_uid(uid)) {
        LOG_VERBOSE("%s:%d - [LMA] Invalid UID", client->ipv4, client->port);
        return ERR_LMA;
    }

    // check if user exists
    if (!exists_user(uid)) {
        LOG_VERBOSE("%s:%d - [LMA] User %s doesn't exist", client->ipv4, client->port, uid);
        sprintf(response, "RMA NOK\n");
        *response_len = 8;
        return 0;
    }
 

    // check if user is logged in
    if(!is_user_logged_in(uid)) {
        LOG_VERBOSE("%s:%d - [LMA] User %s isn't logged in", client->ipv4, client->port, uid);
        sprintf(response, "RMA NLG\n");
        *response_len = 8;
        return 0;
    }

    // update database
    update_database();

    /**
    * Start creating my auctions response
    */
    sprintf(response, "RMA OK");
    *response_len = 6;
    // write response

    int written = get_user_auctions(uid, response);
    // error reading user auctions
    if (written < 0) {
        LOG_VERBOSE("%s:%d - [LMA] Internal error processing user %s auctions", client->ipv4, client->port, uid);
        return ERR_LMA;
    }

    // user has no auctions
    if (written == 1) {
        LOG_VERBOSE("%s:%d - [LMA] No aucions for user %s", client->ipv4, client->port, uid);
        sprintf(response, "RMA NOK\n");
        *response_len = 8;
        return 0;
    }

    LOG_VERBOSE("%s:%d - [LMB] Sent user %s auction list to client", client->ipv4, client->port, uid);

    *response_len += written;
    return 0;
}


/**
 * Lists all the bids done by a user
*/
int handle_my_bids(char *input, struct udp_client *client, char *response, size_t *response_len) {
    LOG_DEBUG("entered handle_my_bids");

    char *uid = strtok(input, "\n");
    if (uid == NULL) {
        LOG_VERBOSE("%s:%d - [LMB] No UID supplied", client->ipv4, client->port);
        return ERR_MB;
    }
    if (!is_valid_uid(uid)) {
        LOG_VERBOSE("%s:%d - [LMB] Invalid UID", client->ipv4, client->port);
        return ERR_MB;
    }

    /**
    * Validate user and in database 
    */
    // check if user exists
    if (!exists_user(uid)) {
        LOG_VERBOSE("%s:%d - [LMA] User %s doesn't exist", client->ipv4, client->port, uid);
        sprintf(response, "RMB NOK\n");
        *response_len = 8;
        return 0;
    }
 
    // check if user is logged in
    if(!is_user_logged_in(uid)) {
        LOG_VERBOSE("%s:%d - [LMB] User %s isn't logged in", client->ipv4, client->port, uid);
        sprintf(response, "RMB NLG\n");
        *response_len = 8;
        return 0;
    }

    // update database
    update_database();

    /**
    * Start creating my bids response
    */
    sprintf(response, "RMB OK");
    *response_len = 6;

    int written = get_user_bids(uid, response);
    if (written < 0) {
        LOG_VERBOSE("%s:%d - [LMB] Internal error processing user %s bids", client->ipv4, client->port, uid);
        return ERR_MB;
    }

    if (written == 1) {
        LOG_VERBOSE("%s:%d - [LMB] User %s has no bids", client->ipv4, client->port, uid);
        sprintf(response, "RMB NOK\n");
        *response_len = 8;
        return 0;
    }

    LOG_VERBOSE("%s:%d - [LMB] Sent user %s bids list to client", client->ipv4, client->port, uid);

    *response_len += written;
    return 0;
}


int handle_list(char *input, struct udp_client *client, char *response, size_t *response_len) {
    LOG_DEBUG("%s:%d - [LST] Entered handler", client->ipv4, client->port);
    // update database
    update_database();
    /**
    * Build and send response
    */
    sprintf(response, "RLS OK");
    *response_len = 6;

    // get auctions list
    int written = get_auctions_list(response);
    if (written < 0) {
        LOG_VERBOSE("%s:%d - [LST] Internal error processing auctions list", client->ipv4, client->port);
        return ERR_LST;
    }

    if (written == 1) {
        LOG_VERBOSE("%s:%d - [LST] No aucions in server", client->ipv4, client->port);
        sprintf(response, "RLS NOK\n");
        *response_len = 8;
        return 0;
    }


    LOG_VERBOSE("%s:%d - [LST] Sent auction list to client", client->ipv4, client->port);

    *response_len += written;
    return 0;
}


int handle_show_record(char *input, struct udp_client *client, char *response, size_t *response_len) {
    LOG_DEBUG("%s:%d - [SRC] Entered handler", client->ipv4, client->port);
    /**
    * Validate command arguments 
    */
    char *aid;
    aid = strtok(input, "\n");
    if (aid == NULL) {
        LOG_VERBOSE("%s:%d - [SRC] No UID supplied", client->ipv4, client->port);
        return ERR_SRC; 
    }

    if (!is_valid_aid(aid)) {
        LOG_VERBOSE("%s:%d - [SRC] Invalid UID", client->ipv4, client->port);
        return ERR_SRC;
    }

    if (!exists_auction(aid)) {
        LOG_VERBOSE("%s:%d - [SRC] Auction %s doesn't exist", client->ipv4, client->port, aid);
        sprintf(response, "RRC NOK\n");
        *response_len = 8;
        return 0;
    }

    // update database
    update_database();

    /**
    * Create show record response
    */
    char auction_info[128];
    if (get_auction_info(aid, auction_info, 128) != 0) {
        LOG_VERBOSE("%s:%d - [SRC] Internal error processing auction %s information", client->ipv4, client->port, aid);
        return ERR_SRC;
    }

    // validate database values (these shouldn't be wrong, but if they are db is corrupted)
    char *host_uid = strtok(auction_info, " ");
    if (host_uid == NULL) {
        LOG_DEBUG("No host UID");
        return ERR_SRC;
    }

    if (!is_valid_uid(host_uid)) {
        LOG_DEBUG("Invalid host UID");
        return ERR_SRC;
    }

    char *asset_name = strtok(NULL, " ");
    if (asset_name == NULL) {
        LOG_DEBUG("No asset name");
        return ERR_SRC;
    }

    if (!is_valid_name(asset_name)) {
        LOG_DEBUG("Invalid asset name");
        return ERR_SRC;
    }
    
    char *fname = strtok(NULL, " ");
    if (fname == NULL) {
        LOG_DEBUG("No asset fname");
        return ERR_SRC;
    }

    if (!is_valid_fname(fname)) {
        LOG_DEBUG("Invalid asset fname");
        return ERR_SRC;
    }

    char *sv = strtok(NULL, " ");
    if (sv == NULL) {
        LOG_DEBUG("No start value");
        return ERR_SRC;
    }

    if (!is_valid_start_value(sv)) {
        LOG_DEBUG("Invalid start value")
        return ERR_SRC;
    }

    char *ta = strtok(NULL, " ");
    if (ta == NULL) {
        LOG_DEBUG("No time active");
        return ERR_SRC;
    }

    if (!is_valid_time_active(ta)) {
        LOG_DEBUG("Invalid time active");
        return ERR_SRC;
    }

    char *start_date = strtok(NULL, " ");
    if (start_date == NULL) {
        LOG_DEBUG("No start date");
        return ERR_SRC;
    }

    char *start_time = strtok(NULL, " ");
    if (start_time == NULL) {
        LOG_DEBUG("No start time")
        return ERR_SRC;
    }

    if (!is_valid_date_time(start_date, start_time)) {
        LOG_DEBUG("Invalid start datetime");
        return ERR_SRC;
    }

    char *start_sec_time = strtok(NULL, "\n");
    if (start_sec_time == NULL) {
        LOG_DEBUG("No start sec time");
        return ERR_SRC;
    }

    sprintf(response, "RRC OK %s %s %s %s %s %s %s", 
                                        host_uid, asset_name, fname,
                                        sv, start_date, start_time, ta);

    *response_len = strlen(response);

    int written = get_auction_bidders_list(aid, response);
    if (written < 0) {
        LOG_VERBOSE("%s:%d - [SRC] Internal error processing auction %s bids", client->ipv4, client->port, aid);
        return ERR_SRC;
    }

    *response_len += written;

    strcat(response, "\n");
    written += 1;

    LOG_VERBOSE("%s:%d - [LST] Sent auction %s record to client", client->ipv4, client->port, aid);

    return 0;
}

