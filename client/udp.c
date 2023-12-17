#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <errno.h>
#include <time.h>

#include "../utils/logging.h"
#include "../utils/validators.h"
#include "../utils/config.h"

#include "command_table.h"
#include "udp.h"

/**
* Requests the AS to log in a user.
* Returns 0 if successfully communicated with server and writes result message to `response`.
* Returns an error code if the operation cannot be concluded or if the server responds with
* an invalid protocol message
*/
int handle_login (char *input, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG("Entered");
    if (client->logged_in)
        return ERR_ALREADY_LOGGED_IN;

    char *uid, *passwd;
    /**
    * Validate command arguments 
    */
    uid = strtok(input, " ");
    if (uid == NULL)
        return ERR_NULL_ARGS;

    if (!is_valid_uid(uid))
        return ERR_INVALID_UID;

    passwd = strtok(NULL, " "); 
    if (passwd == NULL)
        return ERR_NULL_ARGS;

    if (!is_valid_passwd(passwd))
        return ERR_INVALID_PASSWD;

    /**
    * Create and send protocol message
    * Format: LIN UID password
    */
    char message[32];
    sprintf(message, "LIN %.6s %.8s\n", uid, passwd);
    if (send_udp_request(message, strlen(message), client) != 0)
        return ERR_REQUESTING_UDP;

    /**
    * Receive and validate server response
    * Expected format: RLI status
    **/
    char buffer[32] = {0};
    int err = receive_udp_response(buffer, 31, client);
    if (err) {
        if (err == UDP_ERR_NO_LF_MESSAGE)
            return ERR_UNKNOWN_ANSWER;

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return ERR_TIMEDOUT_UDP;

        return ERR_RECEIVING_UDP;
    }

    // ERR, the request was invalid, this means the client is broken
    if (strcmp(buffer, "RLI ERR\n") == 0) {
        strcpy(response, "Server returned an error status for the login command\n");
        return 0;
    }

    // NOK, user failed authentication 
    if (strcmp(buffer, "RLI NOK\n") == 0) {
        strcpy(response, "Incorrect login attempt\n");
        return 0;
    }

    // REG, user registred
    if (strcmp(buffer, "RLI REG\n") == 0) {
        memcpy(client->uid, uid, UID_SIZE);
        memcpy(client->passwd, passwd, PASSWORD_SIZE);
        client->logged_in = 1;
        strcpy(response, "New user registered\n");
        return 0;
    }

    // OK, user was logged in 
    if (strcmp(buffer, "RLI OK\n") == 0) {
        memcpy(client->uid, uid, UID_SIZE);
        memcpy(client->passwd, passwd, PASSWORD_SIZE);
        client->logged_in = 1;
        strcpy(response, "Successful login\n");
        return 0;
    }

    // if none of the above either the client or server are broken (or both!) 
    return ERR_UNKNOWN_ANSWER;
}


/**
* Requests the AS to logout the current client user.
* Returns 0 if successfully communicated with server and writes result to `response`.
* Returns an error code if the operation cannot be concluded or if the server responds with
* an invalid protocol message
*/
int handle_logout (char *input, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG("Entered");
    if (!client->logged_in)
        return ERR_NOT_LOGGED_IN;

    /**
    * Create and send protocol message
    * Format: LOU UID password
    */
    char message[32];
    sprintf(message, "LOU %.6s %.8s\n", client->uid, client->passwd);
    if (send_udp_request(message, strlen(message), client) != 0)
        return ERR_REQUESTING_UDP;

    /**
    * Receive and validate server response
    * Expected format: RLO status
    **/
    char buffer[32] = {0};
    int err = receive_udp_response(buffer, 31, client);
    if (err) {
        if (err == UDP_ERR_NO_LF_MESSAGE)
            return ERR_UNKNOWN_ANSWER;

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return ERR_TIMEDOUT_UDP;

        return ERR_RECEIVING_UDP;
    }

    // ERR, the request was invalid, this means the client is broken
    if (strcmp(buffer, "RLO ERR\n") == 0) {
        strcpy(response, "Server returned an error status for the logout command\n");
        return 0;
    }

    // NOK, the user was not logged in
    // note: the specification of the client + server make this an edge case
    // that should never happen with this client (if the server doesn't log us out mid usage)
    if (strcmp(buffer, "RLO NOK\n") == 0) {
        strcpy(response, "User not logged in\n");
        return 0;
    }

    // UNR, the user is not registred
    if (strcmp(buffer, "RLO UNR\n") == 0) {
        strcpy(response, "Unknown user\n");
        return 0;
    }

    // OK, the user was logged out
    if (strcmp(buffer, "RLO OK\n") == 0) {
        client->logged_in = 0;
        strcpy(response, "Successful logout\n");
        return 0;
    }

    // if none of the above either the client or server are broken (or both!) 
    return ERR_UNKNOWN_ANSWER;
}


/**
* Requests the AS to unregister the current client user.
* Returns 0 if successfully communicated with server and writes result to `response`.
* Returns an error code if the operation cannot be concluded or if the server responds with
* an invalid protocol message
*/
int handle_unregister (char *input, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG("Entered");
    if (!client->logged_in)
        return ERR_NOT_LOGGED_IN;

    /**
    * Create and send protocol message
    * Format: UNR UID password
    */
    char message[32];
    sprintf(message, "UNR %.6s %.8s\n", client->uid, client->passwd);
    if (send_udp_request(message, strlen(message), client) != 0)
        return ERR_REQUESTING_UDP;

    /**
    * Receive and validate server response
    * Expected format: RUR status
    **/
    char buffer[32] = {0};
    int err = receive_udp_response(buffer, 31, client);
    if (err) {
        if (err == UDP_ERR_NO_LF_MESSAGE) {
            return ERR_UNKNOWN_ANSWER;
        }

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return ERR_TIMEDOUT_UDP;

        return ERR_RECEIVING_UDP;
    }

    // ERR, the request was invalid, this means the client is broken
    if (strcmp(buffer, "RUR ERR\n") == 0) {
        strcpy(response, "Server returned an error status for the unregister command\n");
        return 0;
    }

    // UNR, the user was not registered
    // note: the specification of the client + server make this an edge case
    // that should never happen with this client (if the server doesn't unregister us mid usage)
    if (strcmp(buffer, "RUR UNR\n") == 0) {
        strcpy(response, "Unknown user\n");
        return 0;
    }

    // NOK, the user was not logged in
    // note: the specification of the client + server make this an edge case
    // that should never happen with this client (if the server doesn't unregister us mid usage)
    if (strcmp(buffer, "RUR NOK\n") == 0) {
        strcpy(response, "Incorrect unregister attempt\n");
        return 0;
    }

    // OK, user was unregistered 
    if (strcmp(buffer, "RUR OK\n") == 0) {
        client->logged_in = 0;
        strcpy(response, "Successful unregister\n");
        return 0;
    }

    // if none of the above either the client or server are broken (or both!) 
    return ERR_UNKNOWN_ANSWER;
}


/**
* Close the client session. 
* End the current session if the user is not logged in. This handler is a local
* command and doesn't perform any communication with the server.
*/
int handle_exit (char *input, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    if (client->logged_in)
        return ERR_NOT_LOGGED_OUT;

    DISPLAY_CLIENT("Closing sesssion\n");
    free_client(client);
    exit(0);
}

/**
* Requests the AS for the list of existing auctions.
* Returns 0 if successfully communicated with server and writes result to `response`.
* Returns an error code if the operation cannot be concluded or if the server responds with
* an invalid protocol message
*/
int handle_list (char *input, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG("Entered");
    /**
    * Create and send protocol message
    * Format: LST 
    */
    char request[8];
    sprintf(request, "LST\n");
    if (send_udp_request(request, strlen(request), client) != 0) {
        return ERR_REQUESTING_UDP;
    }

    /**
    * Receive and validate server response
    * Expected format: RLS status [AID state]* 
    **/
    char buffer[8192] = {0}; 
    int err = receive_udp_response(buffer, 8191, client);
    if (err) {
        if (err == UDP_ERR_NO_LF_MESSAGE)
            return ERR_UNKNOWN_ANSWER;

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return ERR_TIMEDOUT_UDP;

        return ERR_RECEIVING_UDP;
    }

    // retrieve response protocol command 
    char *cmd_resp = strtok(buffer, " ");
    if (cmd_resp == NULL)
        return ERR_UNKNOWN_ANSWER;

    // validate response protocol command
    if (strcmp(cmd_resp, "RLS") != 0)
        return ERR_UNKNOWN_ANSWER;

    // retrieve command payload
    char *payload = strtok(NULL, "\n");
    if (payload == NULL)
        return ERR_UNKNOWN_ANSWER;

    // ERR, the request was invalid, this means the client is broken
    if (strcmp(payload, "ERR") == 0) {
        strcpy(response, "Server returned an error status for the LST command\n");
        return 0;
    }

    // NOK, no auctions 
    if (strcmp(payload, "NOK") == 0) {
        strcpy(response, "No auctions currently available\n");
        return 0;
    }

    // check if OK is really there (it should) 
    char *status = strtok(payload, " ");
    if (status == NULL)
        return ERR_UNKNOWN_ANSWER;

    // status wasn't OK (meaning we didn't get a known status)
    if (strcmp(status, "OK") != 0)
        return ERR_UNKNOWN_ANSWER;

    // retrieve the actual list from the message
    char *data = strtok(NULL, "\n");
    if (data == NULL)
        return ERR_UNKNOWN_ANSWER;

    /**
    * Build response string in the format:
    * { | AID - State | 5* } (6 per line)
    * Status: A - Active, C - Closed
    */    
    int per_line_count = 0;
    memset(response, 0, MAX_SERVER_RESPONSE);
    strcat(response, 
                     "Format: |{AID - Status}| 5* (6 per line)\n"
                     "Status: A - Active, C - Closed\n\n"

                     "AS Auctions:\n");
    char *auc_id = strtok(data, " ");
    while (auc_id  != NULL) {
        strcat(response, "|");
        strcat(response, auc_id);

        char *status = strtok(NULL, " ");
        // 1 or 0
        if (status == NULL)
            return ERR_UNKNOWN_ANSWER;

        if (strcmp(status, "1") == 0)
            strcat(response, " - A| ");
        else if (strcmp(status, "0") == 0)
            strcat(response, " - C| ");
        else
            return ERR_UNKNOWN_ANSWER;

        // add \n every 6 entries
        per_line_count += 1;
        if (per_line_count == 6) {
            strcat(response, "\n");
            per_line_count = 0;
        }

        auc_id = strtok(NULL, " ");
    }

    // add final \n if necessary
    if (per_line_count != 0)
        strcat(response, "\n");

    return 0;
}


/**
* Requests the AS for the list of a user's auctions.
* Returns 0 if successfully communicated with server and writes result to `response`.
* Returns an error code if the operation cannot be concluded or if the server responds with
* an invalid protocol message
*/
int handle_my_auctions (char *input, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG("Entered");
    if (!client->logged_in){
        return ERR_NOT_LOGGED_IN;
    }

    /**
    * Create and send protocol message
    * Format: LMA UID 
    */
    char request[16];
    sprintf(request, "LMA %.6s\n", client->uid);
    if (send_udp_request(request, strlen(request), client) != 0) {
        return ERR_REQUESTING_UDP;
    }

    /**
    * Receive and validate server response
    * Expected format: RMA status [AID state]* 
    **/
    char buffer[8192] = {0}; 
    int err = receive_udp_response(buffer, 8191, client);
    if (err) {
        if (err == UDP_ERR_NO_LF_MESSAGE)
            return ERR_UNKNOWN_ANSWER;

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return ERR_TIMEDOUT_UDP;

        return ERR_RECEIVING_UDP;
    }

    // retrieve response protocol command 
    char *command = strtok(buffer, " ");
    if (command == NULL)
        return ERR_UNKNOWN_ANSWER;

    // validate response protocol command
    if (strcmp(command, "RMA") != 0)
        return ERR_UNKNOWN_ANSWER;

    char *payload = strtok(NULL, "\n");
    if (payload == NULL)
        return ERR_UNKNOWN_ANSWER;

    // ERR, the request was invalid, this means the client is broken
    if (strcmp(payload, "ERR") == 0) {
        strcpy(response, "Server returned an error status for the myauctions command\n");
        return 0;
    }

    // NOK, no auctions 
    if (strcmp(payload, "NOK") == 0) {
        strcpy(response, "No auctions currently available\n");
        return 0;
    }

    // NLG, user is not logged in
    if (strcmp(payload, "NLG") == 0) {
        strcpy(response, "User is not logged in\n");
        return 0;
    }

    // check if OK is really there (it should) 
    char *status = strtok(payload, " ");
    if (status == NULL)
        return ERR_UNKNOWN_ANSWER;

    // status wasn't OK (meaning we didn't get a known status)
    if (strcmp(status, "OK") != 0)
        return ERR_UNKNOWN_ANSWER;

    // retrieve the actual list from the message
    char *data = strtok(NULL, "\0");
    if (data == NULL)
        return ERR_UNKNOWN_ANSWER;

    /**
    * Build response string in the format:
    * { | AID - State | 5* } (6 per line)
    * Status: A - Active, C - Closed
    */    
    int per_line_count = 0;
    char *auc_id = strtok(data, " ");
    memset(response, 0, MAX_SERVER_RESPONSE);
    strcat(response, 
                     "Format: |{AID - Status}| 5* (6 per line)\n"
                     "Status: A - Active, C - Closed\n\n"
            
                     "Auctions for user ");
    strncat(response, client->uid, UID_SIZE);
    strcat(response, "\n");
    while (auc_id != NULL) {
        if (!is_valid_aid(auc_id))
            return ERR_UNKNOWN_ANSWER;

        strcat(response, "|");
        strcat(response, auc_id);

        char *status = strtok(NULL, " ");
        if (status == NULL)
            return ERR_UNKNOWN_ANSWER;

        if (strcmp(status, "1") == 0)
            strcat(response, " - A| ");
        else if (strcmp(status, "0") == 0)
            strcat(response, " - C| ");
        else
            return ERR_UNKNOWN_ANSWER;

        per_line_count += 1;
        if (per_line_count == 6) {
            strcat(response, "\n");
            per_line_count = 0;
        }

        auc_id = strtok(NULL, " ");
    }

    // add final \n
    if (per_line_count != 0)
        strcat(response, "\n");

    return 0;
}


// TODO revise, it was done without much care 
int handle_show_record (char *input, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG("Entered");
    char *aid;
    /**
    * Validate arguments
    */
    aid = strtok(input, " ");
    if (aid == NULL)
        return ERR_NULL_ARGS;

    if (!is_valid_aid(aid))
        return ERR_INVALID_AID;

    /**
    * Send request to server
    */
    char request[16];
    sprintf(request, "SRC %.3s\n", aid);
    if (send_udp_request(request, strlen(request), client) != 0) {
        return ERR_REQUESTING_UDP;
    }

    /**
    * Receive and validate server response
    * Expected format: 
    * RRC status [host_UID auction_name asset_fname start_value start_date-time timeactive] [BID INFO]
    * [BID INFO] comments explaining in parse
    **/
    char buffer[65535] = {0}; // this reponse might fill an entire datagram... 
    int err = receive_udp_response(buffer, 65534, client);
    if (err) {
        if (err == UDP_ERR_NO_LF_MESSAGE)
            return ERR_UNKNOWN_ANSWER;

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return ERR_TIMEDOUT_UDP;

        return ERR_RECEIVING_UDP;
    }

    // retrieve response protocol command 
    char *command = strtok(buffer, " ");
    if (command == NULL)
        return ERR_UNKNOWN_ANSWER;

    // command response wasn't RRC
    if (strcmp(command, "RRC") != 0)
        return ERR_UNKNOWN_ANSWER;

    char *payload = strtok(NULL, "\n");
    if (payload == NULL)
        return ERR_UNKNOWN_ANSWER;

    // ERR status response
    if (strcmp(payload, "ERR") == 0) {
        strcpy(response, "Server returned an error status for the showrecord command\n");
        return 0;
    }

    // NOK status response
    if (strcmp(payload, "NOK") == 0) {
        strcpy(response, "Auction doesnt exist\n");
        return 0;
    }

    char *status = strtok(payload, " ");
    if (status == NULL)
        return ERR_UNKNOWN_ANSWER;

    if (strcmp(status, "OK") != 0)
        return ERR_UNKNOWN_ANSWER;

    // the actual data with bids information
    char *data = strtok(NULL, "\n");
    if (data == NULL)
        return ERR_UNKNOWN_ANSWER;

    // host_UID
    char *host_uid = strtok(data, " ");
    if (host_uid == NULL)
        return ERR_UNKNOWN_ANSWER;

    if (!is_valid_uid(host_uid))
        return ERR_UNKNOWN_ANSWER;

    // auction name
    char *name = strtok(NULL, " ");
    if (name == NULL)
        return ERR_UNKNOWN_ANSWER;

    if (!is_valid_name(name))
        return ERR_UNKNOWN_ANSWER;

    // asset file name
    char *fname = strtok(NULL, " ");
    if (fname == NULL)
        return ERR_UNKNOWN_ANSWER;

    if (!is_valid_fname(fname))
        return ERR_UNKNOWN_ANSWER;

    // start value
    char *sv = strtok(NULL, " ");
    if (sv == NULL)
        return ERR_UNKNOWN_ANSWER;

    if (!is_valid_start_value(sv))
        return ERR_UNKNOWN_ANSWER;

    // start date time in format YYYY-MM-DD HH:MM:SS
    char *start_date = strtok(NULL, " ");
    if (start_date == NULL)
        return ERR_UNKNOWN_ANSWER;

    char *start_time = strtok(NULL, " ");
    if (start_time == NULL)
        return ERR_UNKNOWN_ANSWER;

    if (!is_valid_date_time(start_date, start_time))
        return ERR_UNKNOWN_ANSWER;

    char *time_active = strtok(NULL, " ");
    if (time_active == NULL)
        return ERR_UNKNOWN_ANSWER;

    if (!is_valid_time_active(time_active))
        return ERR_UNKNOWN_ANSWER;

    /**
    * Build cliente display message
    */

    // auction information
    char *record_fmt_str = 
        "Auction:        %s\n"
        "Auction name:   %s\n"
        "Hosted by:      %s\n"
        "Asset:          %s\n"
        "Start Value:    %s\n"
        "Date started:   %s %s\n"
        "Time Active:    %s\n";

    sprintf(response, record_fmt_str, 
                            aid, name, host_uid, fname, 
                            sv, start_date, start_time, time_active);

    /**
    * Build bids information in the format:
    * { | host UID - bid value - bid time - bid sec time | 2* } (6 per line)
    */
    char *token = strtok(NULL, " ");
    int per_line_count = 0;

    char bids_str[16384] = {0};
    char bid_buff[64];
    while (token != NULL && strcmp(token, "B") == 0) {
        // parse a bidder information 
        char *bidder_uid = strtok(NULL, " ");
        if (bidder_uid == NULL)
            return ERR_UNKNOWN_ANSWER;

        if (!is_valid_uid(bidder_uid))
            return ERR_UNKNOWN_ANSWER;

        char *bid_value = strtok(NULL, " ");
        if (bid_value == NULL)
            return ERR_UNKNOWN_ANSWER; 

        if (!is_valid_start_value(bid_value))
            return ERR_UNKNOWN_ANSWER;

        char *bid_date = strtok(NULL, " ");
        if (bid_date == NULL)
            return ERR_UNKNOWN_ANSWER;
            
        char *bid_time = strtok(NULL, " ");
        if (bid_time == NULL)
            return ERR_UNKNOWN_ANSWER;
        
        if (!is_valid_date_time(bid_date, bid_time))
            return ERR_UNKNOWN_ANSWER;

        char *bid_sec = strtok(NULL, " ");
        if (bid_sec == NULL)
            return ERR_UNKNOWN_ANSWER;

        // the validation for start value works the same as of bid_sec (numeric w len <= 6)
        if (!is_valid_start_value(bid_sec))
            return ERR_UNKNOWN_ANSWER;

        sprintf(bid_buff, "|%s - %s - %s %s - %s| ", bidder_uid, bid_value, bid_date, bid_time, bid_sec);
        strcat(bids_str, bid_buff);
        per_line_count += 1;

        if (per_line_count == 2) {
            per_line_count = 0;
            strcat(bids_str, "\n");
        }

        token = strtok(NULL, " ");
    }

    if (per_line_count != 2) 
        strcat(bids_str, "\n");

    // check for E entry
    char auction_status[128];
    if (token != NULL) {
        if (strcmp(token, "E") != 0)
            return ERR_UNKNOWN_ANSWER;
        
        char *end_date = strtok(NULL, " ");
        if (end_date == NULL)
            return ERR_UNKNOWN_ANSWER;

        char *end_time = strtok(NULL, " ");
        if (end_time == NULL)
            return ERR_UNKNOWN_ANSWER;

        if (!is_valid_date_time(end_date, end_time))
            return ERR_UNKNOWN_ANSWER;

        char *end_sec_time = strtok(NULL, "\n");
        if (end_sec_time == NULL)
            return ERR_UNKNOWN_ANSWER;

        if (!is_valid_start_value(end_sec_time))
            return ERR_UNKNOWN_ANSWER;

        
        sprintf(auction_status, "Status:         Closed\n"
                                "End Date:       %s %s\n"
                                "End Sec Time:   %s\n", end_date, end_time, end_sec_time);
    } else {
        sprintf(auction_status, "Status: Active\n");
    }

    strcat(response, auction_status);
    strcat(response, "Bids:\n");
    strcat(response, bids_str);

    return 0;
}


/**
 * Lists all the binds done by the client (listed 5 in each line)
*/
int handle_my_bids (char *input, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG("Entered my bids");

    if (!client->logged_in) {
        return ERR_NOT_LOGGED_IN;
    } 

    char request[16];
    sprintf(request, "LMB %.6s\n", client->uid);
    if (send_udp_request(request, strlen(request), client) != 0) {
        return ERR_REQUESTING_UDP;
    }

    /**
    * Receive and validate server response
    * Expected format: RLB status [AID state]* 
    **/
    char buffer[8192] = {0}; 
    int err = receive_udp_response(buffer, 8191, client);
    if (err) {
        if (err == UDP_ERR_NO_LF_MESSAGE)
            return ERR_UNKNOWN_ANSWER;

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return ERR_TIMEDOUT_UDP;

        return ERR_RECEIVING_UDP;
    }

    // retrieve response protocol command 
    char *cmd_resp = strtok(buffer, " ");
    if (cmd_resp == NULL)
        return ERR_UNKNOWN_ANSWER;

    // validate response protocol command
    if (strcmp(cmd_resp, "RMB") != 0)
        return ERR_UNKNOWN_ANSWER;

    // retrieve command payload
    char *payload = strtok(NULL, "\n");
    if (payload == NULL)
        return ERR_UNKNOWN_ANSWER;

    // ERR, the request was invalid, this means the client is broken
    if (strcmp(payload, "ERR") == 0) {
        strcpy(response, "Server returned an error status for the mybids command\n");
        return 0;
    }

    // NOK, no auctions 
    if (strcmp(payload, "NOK") == 0) {
        strcpy(response, "No bids placed yet\n");
        return 0;
    }

    // check if OK is really there (it should) 
    char *status = strtok(payload, " ");
    if (status == NULL)
        return ERR_UNKNOWN_ANSWER;

    // status wasn't OK (meaning we didn't get a known status)
    if (strcmp(status, "OK") != 0)
        return ERR_UNKNOWN_ANSWER;

    // retrieve the actual list from the message
    char *data = strtok(NULL, "\0");
    if (data == NULL)
        return ERR_UNKNOWN_ANSWER;

    /**
    * Build response string in the format:
    * { | AID - State | 5* } (6 per line)
    * Status: A - Active, C - Closed
    */    
    int per_line_count = 0;
    memset(response, 0, MAX_SERVER_RESPONSE);
    strcat(response, 
                     "Format: |{AID - Status}| 5* (6 per line)\n"
                     "Status: A - Active, C - Closed\n\n"

                     "AS Auctions with user bids:\n");
    char *auc_id = strtok(data, " ");
    while (auc_id  != NULL) {
        strcat(response, "|");
        strcat(response, auc_id);

        char *status = strtok(NULL, " ");
        // 1 or 0
        if (status == NULL)
            return ERR_UNKNOWN_ANSWER;

        if (strcmp(status, "1") == 0)
            strcat(response, " - A| ");
        else if (strcmp(status, "0") == 0)
            strcat(response, " - C| ");
        else
            return ERR_UNKNOWN_ANSWER;

        per_line_count += 1;
        if (per_line_count == 6) {
            strcat(response, "\n");
            per_line_count = 0;
        }

        auc_id = strtok(NULL, " ");
    }

    // add final \n
    if (per_line_count != 0)
        strcat(response, "\n");

    return 0;
}

/**
* Send a the UDP message `message` with size `msg_size` to the server.
* Returns 0 on sucess and -1 on failure, to detect timeout check errno value.
*/
int send_udp_request(char *message, size_t msg_size, struct client_state *client) {
    ssize_t n = sendto(client->annouce_socket, message, msg_size, 0, client->as_addr, client->as_addr_len);
    if (n == -1) {
        LOG_DEBUG("Failed sending UDP message");
        LOG_DEBUG("sendto: %s", strerror(errno));
        return -1;
    }

    return 0;
}

/**
* Read a message from the client's UDP socket to `buffer` of size `msg_size`.
* It will validate if this message is following to the protocol by checking if it
is LF terminated, it it isn't it will return UDP_ERR_NO_LF_MESSAGE.
* If UDP is sucessfully read and obeys the protocol 0 is returned, else, an error code is returned.
*/
int receive_udp_response(char *buffer, size_t response_size, struct client_state *client) {
   // receive response
    ssize_t n = recvfrom(client->annouce_socket, buffer, response_size, 0, client->as_addr, &client->as_addr_len);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            LOG_DEBUG("Timed out waiting for UDP response");
        } else {
            LOG_DEBUG("Failed reading from UDP socket");
            LOG_DEBUG("recvfrom: %s", strerror(errno));
        }
        return 1;
    }

    if (n == 0) {
        LOG_DEBUG("Empty message");
        return 1;
    }

    /**
    * NOTE: Yes, we are being that strict with the protocol...
    */
    if (buffer[n - 1] != '\n') {
        return UDP_ERR_NO_LF_MESSAGE;
    }

    return 0;
}
