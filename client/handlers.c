#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>

#include "../utils/logging.h"
#include "../utils/validators.h"
#include "../utils/config.h"

#include "command_error.h"
#include "handlers.h"
#include "client.h"
#include "parser.h"

int send_udp_request(char *message, size_t n_msg_bytes, struct client_state *client) {
    size_t n = sendto(client->annouce_socket, message, n_msg_bytes, 0, client->as_addr, client->as_addr_len);
    if (n == -1) {
        LOG_DEBUG("sendto fail")
        LOG_ERROR("failed while sending login response");
        return -1;
    }
    return 0;
}

int receive_udp_response(char *buffer, size_t response_size, struct client_state *client) {
    //set timeout to socket
    struct timeval timeout;
    timeout.tv_sec = 300;  // Timeout in seconds
    timeout.tv_usec = 0; // Additional microseconds
    if (setsockopt(client->annouce_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        LOG_DEBUG("failed setsockopt")
        LOG_ERROR("Error setting socket timeout");
        return -1;
    }
    //receive response
    size_t n = recvfrom(client->annouce_socket, buffer, response_size, 0, client->as_addr, &client->as_addr_len);
    if (n == -1) {
        LOG_DEBUG("failed recvfrom")
        LOG_ERROR("failed while receiving response");
        return -1;
    }
    if (n == 0){
        LOG_ERROR("socket timed out\n");
        return -1;
    }
    buffer[n] = '\0';
    return 0;
}


/*--------------------------------- LOGIN ------------------------------------------------------------------*/

int handle_login (struct arg *args, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG("entered handle_login")

    if (client->logged_in) {
        LOG_DEBUG("client->logged_in = 1")
        REPLY(response, "already logged in");
        return 0;
    }

    // this might not be needed because args is always initialized, even if its value is NULL
    if (args == NULL) {
        LOG_DEBUG("NULL first arg")
        return ERROR_NULL_ARGS;
    }

    char *uid = args->value;
    if (uid == NULL)
        return ERROR_NULL_UID;
    if (!is_valid_uid(uid))
        return ERROR_INVALID_UID;

    char *passwd = args->next_arg->value;
    if (passwd == NULL)
        return ERROR_NULL_PASSWD;
    if (!is_valid_passwd(passwd))
        return ERROR_INVALID_PASSWD;

    //Create protocol message format: LIN UID password
    char message[MAX_LOGIN_COMMAND];
    snprintf(message, MAX_LOGIN_COMMAND, "LIN %s %s\n", uid, passwd);

    //comunicate with server
    if (send_udp_request(message, MAX_LOGIN_COMMAND, client)!=0)
        return ERROR_LOGIN;
    char buffer[MAX_LOGIN_RESPONSE];
    if (receive_udp_response(buffer, MAX_LOGIN_RESPONSE, client)!=0)
        return ERROR_LOGIN;

    //check result
    if (!strcmp(buffer, SUCESSFULL_LOGIN)){
        client->logged_in = 1;
        REPLY(response, "sucessfull login");
        return 0;
    }
    if (!strcmp(buffer, UNSUCESSFULL_LOGIN)){
        REPLY(response, "incorrect login attempt");
        return 0;
    }
    if (!strcmp(buffer, SUCESSFULL_REGISTER)){
        memcpy(client->uid, uid, UID_SIZE);
        memcpy(client->passwd, passwd, PASSWORD_SIZE);
        client->logged_in = 1;
        REPLY(response, "new user registered");
        return 0;
    }
    return ERROR_UNKNOWN_ANSWER;
}

/*--------------------------------- LOGOUT ------------------------------------------------------------------*/

int handle_logout (struct arg *args, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    if (!client->logged_in) {
        REPLY(response, "already logged out");
        return 0;
    }

    //Create protocol message format: LOU UID password format
    char message[MAX_LOGOUT_COMMAND];
    snprintf(message, MAX_LOGOUT_COMMAND, "LOU %s %s\n", client->uid, client->passwd); 

    //Communicate with the server
    if (send_udp_request(message, MAX_LOGOUT_COMMAND, client)!=0)
        return ERROR_LOGOUT;
    char buffer[MAX_LOGOUT_RESPONSE];
    if (receive_udp_response(buffer, MAX_LOGOUT_RESPONSE, client)!=0)
        return ERROR_LOGOUT;
    
    //check result
    if (!strcmp(buffer, SUCESSFULL_LOGOUT)){
        client->logged_in = 0;
        REPLY(response, "sucessfull logout");
        return 0;
    }
    if (!strcmp(buffer, UNSUCESSFULL_LOGOUT)){
        REPLY(response, "user not logged in");
        return 0;
    }
    if (!strcmp(buffer, UNREGISTERED_LOGOUT)){
        REPLY(response, "unknown user");
        return 0;
    }
    return ERROR_UNKNOWN_ANSWER;
}


//TODO-----------------------------------------------------------------------------------------------
int handle_unregister (struct arg *args, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_exit (struct arg *args, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_open (struct arg *args, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_close (struct arg *args, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_my_auctions (struct arg *args, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_my_bids (struct arg *args, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_list (struct arg *args, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_show_asset (struct arg *args, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_bid (struct arg *args, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_show_record (struct arg *args, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG(" ");
    return 0;
}

