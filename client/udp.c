#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <errno.h>


#include "../utils/logging.h"
#include "../utils/validators.h"

#include "command_table.h"
/*--------------------------------- LOGIN --------------------------------------*/

int send_udp_request(char *buff, size_t size, struct client_state *);
int receive_udp_response(char *buff, size_t size, struct client_state *);

int handle_login (char *input, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG("Entered");
    if (client->logged_in)
        return ERR_ALREADY_LOGGED_IN;

    char *uid = strtok(input, " ");

    if (uid == NULL)
        return ERR_NULL_ARGS;

    if (!is_valid_uid(uid))
        return ERR_INVALID_UID;

    char *passwd = strtok(NULL, " "); 

    if (passwd == NULL)
        return ERR_NULL_ARGS;

    if (!is_valid_passwd(passwd))
        return ERR_INVALID_PASSWD;

    // create protocol message format: LIN UID password
    char message[64];
    sprintf(message, "LIN %.6s %.8s\n", uid, passwd);

   // communicate with the server
    if (send_udp_request(message, strlen(message), client) != 0)
        return ERR_REQUESTING_UDP;

    char buffer[32];
    int err = receive_udp_response(buffer, 32, client);
    if (err < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return ERR_TIMEDOUT_UDP;
        }
        return ERR_RECEIVING_UDP;
    }

    /**
    * Check response
    */
    if (!strcmp(buffer, "RLI OK\n")){
        memcpy(client->uid, uid, UID_SIZE);
        memcpy(client->passwd, passwd, PASSWORD_SIZE);
        client->logged_in = 1;
        strcpy(response, "Successful login\n");
        return 0;
    }

    if (!strcmp(buffer, "RLI NOK\n")){
        strcpy(response, "Incorrect login attempt\n");
        return 0;
    }

    if (!strcmp(buffer, "RLI REG\n")){
        memcpy(client->uid, uid, UID_SIZE);
        memcpy(client->passwd, passwd, PASSWORD_SIZE);
        client->logged_in = 1;
        strcpy(response, "New user registered\n");
        return 0;
    }

    return ERR_UNKNOWN_ANSWER;
}

/*--------------------------------- LOGOUT -------------------------------------*/

int handle_logout (char *input, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG("Entered");
    if (!client->logged_in)
        return ERR_NOT_LOGGED_IN;

    // create protocol message format: LOU UID password
    char message[128];
    sprintf(message, "LOU %.6s %.8s\n", client->uid, client->passwd);

    // communicate with the server
    if (send_udp_request(message, strlen(message), client) != 0)
        return ERR_REQUESTING_UDP;

    char buffer[128];
    int err = receive_udp_response(buffer, 128, client);
    if (err < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return ERR_TIMEDOUT_UDP;
        }
        return ERR_RECEIVING_UDP;
    }

    // check result
    if (strcmp(buffer, "RLO OK\n") == 0) {
        client->logged_in = 0;
        strcpy(response, "Successful logout\n");
        return 0;
    }

    if (strcmp(buffer, "RLO NOK\n") == 0) {
        strcpy(response, "User not logged in\n");
        return 0;
    }

    if (strcmp(buffer, "RLO UNR\n") == 0) {
        strcpy(response, "Unknown user\n");
        return 0;
    }

    return ERR_UNKNOWN_ANSWER;
}

/*--------------------------------- UNREGISTER ---------------------------------*/

int handle_unregister (char *input, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG("Entered");
    if (!client->logged_in)
        return ERR_NOT_LOGGED_IN;

    // create protocol message format: UNR UID password
    char message[128];
    sprintf(message, "UNR %.6s %.8s\n", client->uid, client->passwd);

    // communicate with the server
    if (send_udp_request(message, strlen(message), client) != 0)
        return ERR_REQUESTING_UDP;

    char buffer[128];
    int err = receive_udp_response(buffer, 128, client);
    if (err < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return ERR_TIMEDOUT_UDP;
        }
        return ERR_RECEIVING_UDP;
    }
    
    // check result
    if (!strcmp(buffer, "RUR OK\n")) {
        client->logged_in = 0;
        strcpy(response, "Successful unregister\n");
        return 0;
    }

    if (!strcmp(buffer, "RUR UNR\n")) {
        strcpy(response, "Unknown user\n");
        return 0;
    }

    if (!strcmp(buffer, "RUR NOK\n")) {
        strcpy(response, "Incorrect unregister attempt\n");
        return 0;
    }

    return ERR_UNKNOWN_ANSWER;
}

/*--------------------------------- EXIT ----------------------------------------*/

int handle_exit (char *input, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    if (client->logged_in)
        return ERR_NOT_LOGGED_OUT;

    DISPLAY_CLIENT("Closing sesssion\n");
    free_client(client);
    exit(0);
}

/*--------------------------------- LIST ----------------------------------------*/

int handle_list (char *input, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG("Entered");
    /**
    * Build response and send it
    */
    char request[8];
    sprintf(request, "LST\n");

    if (send_udp_request(request, strlen(request), client) != 0) {
        return ERR_REQUESTING_UDP;
    }

    char buffer[8192]; 
    int err = receive_udp_response(buffer, 8192, client);
    if (err < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return ERR_TIMEDOUT_UDP;
        }
        return ERR_RECEIVING_UDP;
    }

    /** 
    * Validate expected arguments and return response as soon as one is found.
    * If invalid arguments are read or arguments are missing return with error
    */
    char *cmd_resp = strtok(buffer, " ");

    if (cmd_resp == NULL) {
        return ERR_UNKNOWN_ANSWER;
    }

    // check if command response matches the sent request
    if (strcmp(cmd_resp, "RLS")) {
        return ERR_UNKNOWN_ANSWER;
    }

    char *payload = strtok(NULL, "\n");

    if (payload == NULL) {
        return ERR_UNKNOWN_ANSWER;
    }

    // check if NOK was received (it's the entire payload)
    if (!strcmp(payload, "NOK")) {
        strcpy(response, "No auctions currently available\n");
        return 0;
    }

    // retrieve (expected) OK status from payload
    char *status = strtok(payload, " ");

    if (status == NULL) {
        return ERR_UNKNOWN_ANSWER;
    }

    // if status is not OK then we are out of options, return error    
    if (strcmp(status, "OK") != 0) {
        return ERR_UNKNOWN_ANSWER;
    }

    // retrieve requested data from payload        
    char *data = strtok(NULL, "\n");

    if (data == NULL) {
        return ERR_UNKNOWN_ANSWER;
    }

    /**
    * Build response string in the format:
    * { | AID - State | 5* } (6 per line)
    * Status: A - Active, C - Closed
    */    
    int per_line_count = 0;
    memset(response, 0, MAX_SERVER_RESPONSE);
    strcat(response, "--- Clearing terminal ---\033[2J\033[HFormat: |{AID - Status}| 5* (6 per line)\nStatus: A - Active, C - Closed\n\n");
    char *auc_id = strtok(data, " ");
    while (auc_id  != NULL) {
        strcat(response, "|");
        strcat(response, auc_id);

        char *status = strtok(NULL, " ");
        // 1 or 0
        if (status == NULL) {
            return ERR_UNKNOWN_ANSWER;
        }

        if (!strcmp(status, "1")) {
            strcat(response, " - A| ");
        } else { // we will consider strange responses also as closed
            strcat(response, " - C| ");
        }

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

/*--------------------------------- MY AUCTIONS ----------------------------------------*/

int handle_my_auctions (char *input, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    char *uid = strtok(input, " ");
    /**
    * Build request and send it
    */
    if (uid == NULL) {
        return ERR_NULL_ARGS;
    }

    if (!is_valid_uid(uid)) {
        return ERR_INVALID_UID;
    }

    char request[16];
    sprintf(request, "LMA %.6s\n", uid);
    if (send_udp_request(request, strlen(request), client) != 0) {
        return ERR_REQUESTING_UDP;
    }

    char buffer[8192]; 
    if (receive_udp_response(buffer, 8192, client) < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return ERR_TIMEDOUT_UDP;
        }
        return ERR_RECEIVING_UDP;
    }

    /** 
    * Validate expected arguments and return response as soon as one is found.
    * If invalid arguments are read or arguments are missing return with error
    */
    char *cmd_resp = strtok(buffer, " ");

    if (cmd_resp == NULL) {
        return ERR_UNKNOWN_ANSWER;
    }

    // check to see if command response matches our request
    if (strcmp(cmd_resp, "RMA")) {
        return ERR_UNKNOWN_ANSWER;
    }

    char *payload = strtok(NULL, "\n");

    if (payload == NULL) {
        return ERR_UNKNOWN_ANSWER;
    }

    // check if NOK was received (it's the entire payload)
    if (!strcmp(payload, "NOK")) {
        strcpy(response, "No auctions currently available\n");
        return 0;
    }

    // check if NLG was received (it's the entire payload)
    if (!strcmp(payload, "NLG")) {
        strcpy(response, "User is not logged in\n");
        return 0;
    }

    // retrieve (expected) OK status from payload
    char *status = strtok(payload, " ");

    if (status == NULL) {
        return ERR_UNKNOWN_ANSWER;
    }

    // if OK was not received then we are out of options, so it's an error
    if (strcmp(status, "OK")) {
        return ERR_UNKNOWN_ANSWER;
    }

    // retrieve requested data from payload
    char *data = strtok(NULL, "\n");

    if (data == NULL) {
        return ERR_UNKNOWN_ANSWER;
    }

    /**
    * Build response string in the format:
    * { | AID - State | 5* } (6 per line)
    * Status: A - Active, C - Closed
    */    
    int per_line_count = 0;
    char *auc_id = strtok(data, " ");
    memset(response, 0, MAX_SERVER_RESPONSE);
    strcat(response, "--- Clearing terminal ---\033[2J\033[HFormat: |{AID - Status}| 5* (6 per line)\nStatus: A - Active, C - Closed\n\n");
    while (auc_id  != NULL) {
        strcat(response, "|");
        strcat(response, auc_id);

        char *status = strtok(NULL, " ");
        if (status == NULL) {
            return ERR_UNKNOWN_ANSWER;
        }

        if (!strcmp(status, "1")) {
            strcat(response, " - A| ");
        } else {
            strcat(response, " - C| ");
        }

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

/*--------------------------------- SHOW RECORD  ----------------------------------------*/

int handle_show_record (char *input, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    char *aid;
    /**
    * Validate arguments
    */
    aid = strtok(input, " ");
    if (aid == NULL) {
        return ERR_NULL_ARGS;
    }

    if (!is_valid_aid(aid)) {
        return ERR_INVALID_AID;
    }

    /**
    * Send request to server
    */
    char request[16];
    sprintf(request, "SRC %.3s\n", aid);
    if (send_udp_request(request, strlen(request), client) != 0) {
        return ERR_REQUESTING_UDP;
    }

    char buffer[MAX_SERVER_RESPONSE] = {0};
    if (receive_udp_response(buffer, MAX_SERVER_RESPONSE, client) < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return ERR_TIMEDOUT_UDP;
        }
        return ERR_RECEIVING_UDP;
    }

    /**
    * Read and validate server response
    */
    char *cmd_resp = strtok(buffer, " ");
    if (cmd_resp == NULL) {
        return ERR_UNKNOWN_ANSWER;
    }

    // command response wasn't RRC
    if (strcmp(cmd_resp, "RRC") != 0) {
        return ERR_UNKNOWN_ANSWER;
    }

    char *payload = strtok(NULL, "\n");
    if (payload == NULL) {
        return ERR_UNKNOWN_ANSWER;
    }

    if (!strcmp(payload, "ERR")) {
        strcpy(response, "Got an error response from server\n");
        return 0;
    }

    // NOK status response
    if (strcmp(payload, "NOK") == 0) {
        strcpy(response, "Auction doesnt exist\n");
        return 0;
    }

    char *status = strtok(payload, " ");

    if (status == NULL) {
        return ERR_UNKNOWN_ANSWER;
    }

    if (strcmp(status, "OK") != 0) {
        return ERR_UNKNOWN_ANSWER;
    }

    char *data = strtok(NULL, "\n");

    if (data == NULL) {
        return ERR_UNKNOWN_ANSWER;
    }

    char *host_uid = strtok(data, " ");

    if (host_uid == NULL) {
        return ERR_UNKNOWN_ANSWER;
    }

    if (!is_valid_uid(host_uid)) {
        return ERR_UNKNOWN_ANSWER;
    }

    char *auc_name = strtok(NULL, " ");

    if (auc_name == NULL) {
        return ERR_UNKNOWN_ANSWER;
    }

    if (!is_valid_name(auc_name)) {
        return ERR_UNKNOWN_ANSWER;
    }

    char *fname = strtok(NULL, " ");

    if (fname == NULL) {
        return ERR_UNKNOWN_ANSWER;
    }

    if (!is_valid_fname(fname)) {
        return ERR_UNKNOWN_ANSWER;
    }

    char *sv = strtok(NULL, " ");

    if (sv == NULL) {
        return ERR_UNKNOWN_ANSWER;
    }

    if (!is_valid_start_value(sv)) {
        return ERR_UNKNOWN_ANSWER;
    }

    char *start_date_time;

    if (start_date_time == NULL) {
        return ERR_UNKNOWN_ANSWER;
    }

    // TODO build the user response from the server response
    // if (!is_valid_date_time(start_date_time)) {
    //     return ERR_UNKNOWN_ANSWER;
    // }


    strcpy(response, data);

    return 0;
}


int handle_my_bids (char *input, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG(" ");
    return 0;
}

int send_udp_request(char *message, size_t msg_size, struct client_state *client) {
    ssize_t n = sendto(client->annouce_socket, message, msg_size, 0, client->as_addr, client->as_addr_len);
    if (n == -1) {
        LOG_DEBUG("Failed sending UDP message");
        LOG_ERROR("sendto: %s", strerror(errno));
        return -1;
    }

    return 0;
}

int receive_udp_response(char *buffer, size_t response_size, struct client_state *client) {
    //set timeout to socket
    struct timeval timeout;
    timeout.tv_sec = 5;  // Timeout in seconds
    timeout.tv_usec = 0; // Additional microseconds
    if (setsockopt(client->annouce_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        LOG_DEBUG("Failed setting timeout for UDP response socket")
        LOG_ERROR("setsockopt: %s", strerror(errno));
        return -1;
    }

    //receive response
    ssize_t n = recvfrom(client->annouce_socket, buffer, response_size, 0, client->as_addr, &client->as_addr_len);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            LOG_DEBUG("Timed out waiting for UDP response");
        } else {
            LOG_DEBUG("Failed reading from UDP socket");
            LOG_ERROR("recvfrom: %s", strerror(errno));
        }
        return -1;
    }

    buffer[n] = '\0';

    return 0;
}
