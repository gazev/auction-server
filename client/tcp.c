#include <asm-generic/errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <unistd.h>

#include "../utils/logging.h"
#include "../utils/validators.h"
#include "../utils/utils.h"
#include "../utils/constants.h"
#include "../utils/config.h"

#include "client.h"
#include "command_table.h"
#include "tcp.h"

/** Wrapper function for open() and setsockopt() with timeout */
int open_tcp_connection_to_server(struct client_state *);

/** Analyze server responses */
int determine_open_response_error(char *status, char *response);
int determine_close_response_error(char *status, char *response);
int determine_show_asset_response_error(char *status, char *response);
int determine_bid_response_error(char *status, char *response);

/** Helper funcs */
int get_show_asset_arg_len(int argno);
void rollback_asset_creation(char *fname);

/**
* Makes an OPA request to the server. 
*
* Returns 0 and writes result to `response` if the equest is made and the server 
* responds with a valid message.

* Returns an error code if:
*   The request can't be made, either because the parameters are invalid or there is a
* connection error, timeout or underlying OS error. 
*   The server responds with an unknown protocol message.
*/
int handle_open (char *input, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    if (!client->logged_in) 
        return ERR_NOT_LOGGED_IN;

    char *name, *asset_fname, *start_value, *time_active;
    /**
    * Validate client arguments
    */
    name = strtok(input, " ");
    if (name == NULL)
        return ERR_NULL_ARGS;

    if (!is_valid_name(name))
        return ERR_INVALID_ASSET_NAME;

    // validate asset file name
    asset_fname = strtok(NULL, " ");
    if (asset_fname == NULL)
        return ERR_NULL_ARGS;

    if (!is_valid_fname(asset_fname))
        return ERR_INVALID_ASSET_FNAME;

    // validate start value
    start_value = strtok(NULL, " ");
    if (start_value == NULL)
        return ERR_NULL_ARGS;

    if (!is_valid_start_value(start_value))
        return ERR_INVALID_SV;

    // validate time active
    time_active = strtok(NULL, " ");
    if (time_active == NULL)
        return ERR_NULL_ARGS;

    if (!is_valid_time_active(time_active))
        return ERR_INVALID_TA;

    /**
    * Get asset file information
    */
    struct stat st; 
    if (stat(asset_fname, &st) != 0) {
        LOG_DEBUG("stat");
        return ERR_INVALID_ASSET_FILE;
    }

    // file is too large
    if (st.st_size > MAX_FSIZE) {
        LOG_DEBUG("too big");
        return ERR_INVALID_ASSET_FILE;
    }

    int asset_fd;
    // if file doesn't exist
    if ((asset_fd = open(asset_fname, O_RDONLY, 0)) < 0) {
        LOG_DEBUG("does not exist");
        return ERR_INVALID_ASSET_FILE;
    }

    /**
    * Open connect to server with a 5s timeout socket
    */
    int conn_fd;
    if ((conn_fd = open_tcp_connection_to_server(client)) < 0)
        return ERR_TCP_CONN_TO_SERVER;

    /**
    * Create and send the request
    * Format: OPA UID password name start_value timeactive Fname Fsize Fdata 
    */

    // send header information
    char auction_info[128] = {0};
    sprintf(auction_info, "OPA %.6s %.8s %s %d %d %s %ld ", 
                            client->uid, client->passwd, name, 
                            atoi(start_value), atoi(time_active), 
                            asset_fname, st.st_size);
    
    if (send_tcp_message(auction_info, strlen(auction_info), conn_fd) != 0) {
        close (asset_fd);
        close(conn_fd);
        if (errno == EPIPE)
            return ERR_TCP_CLOSED_CONN;

        return ERR_SENDING_TCP;
    }

    int err = as_send_asset_file(asset_fd, conn_fd, st.st_size);
    if (err) {
        close(asset_fd);
        close(conn_fd);
        if (errno == EPIPE)
            return ERR_TCP_CLOSED_CONN;

        if (err == ERR_READ_AF)
            return ERR_READ_ASSET_FILE;

        return ERR_SENDING_TCP;
    }

    close(asset_fd);

    /**
    * Read and validate server response 
    * Format: ROA status [AID] 
    * eg: ROA OK 003 
    */
    // read the response command
    char command[8] = {0}; // enough to fit valid responses
    err = read_tcp_stream(command, 4, conn_fd);
    if (err) {
        close(conn_fd);
        // if we timed out reading from stream
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return ERR_TIMEOUT_TCP;

        // if client closed the connection
        if (err == ERR_TCP_READ_CLOSED)
            return ERR_TCP_CLOSED_CONN;

        // other error
        return ERR_RECEIVING_TCP;
    }

    // if command is not ROA, server message is invalid 
    if (strcmp(command, "ROA ")) {
        close(conn_fd);
        return ERR_UNKNOWN_ANSWER;
    }

    // get ROA response status
    char status[4] = {0};
    err = read_tcp_stream(status, 3, conn_fd);
    if (err) {
        close(conn_fd);
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return ERR_TIMEOUT_TCP;

        if (err == ERR_TCP_READ_CLOSED)
            return ERR_TCP_CLOSED_CONN;

        return ERR_RECEIVING_TCP;
    }

    // if we don't receive OK status, determine result for received status 
    if (strcmp(status, "OK ") != 0) {
        // LF is no terminating the message so it's invalid
        if (!is_lf_in_stream(conn_fd)) // return w comma operator for readibilty's sake
            return close(conn_fd), ERR_UNKNOWN_ANSWER;

        close(conn_fd);
        return determine_open_response_error(status, response);
    }

    // read AID
    char aid_buff[4] = {0};
    err = read_tcp_stream(aid_buff, 3, conn_fd);
    if (err) {
        close(conn_fd);
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return ERR_TIMEOUT_TCP;

        if (err == ERR_TCP_READ_CLOSED)
            return ERR_TCP_CLOSED_CONN;

        return ERR_RECEIVING_TCP;
    }

    // if message isn't termiated with \n
    if (!is_lf_in_stream(conn_fd)) {
        close(conn_fd);
        return ERR_UNKNOWN_ANSWER;
    }

    /**
    * Validate server response arguments
    */
    char *aid = strtok(aid_buff, "\n");
    if (aid == NULL) {
        close(conn_fd);
        return ERR_UNKNOWN_ANSWER;
    }
 
    if (!is_valid_aid(aid)) {
        close(conn_fd);
        return ERR_UNKNOWN_ANSWER;
    }

    close(conn_fd);
    sprintf(response, "Asset %.3s successfully started by the AS\n", aid);

    return 0;
}

/**
* Analyses a not OK response sent by the server to the OPA command.
* If it's a valid response returns 0 and writes corresponding result to `response`.
* If the message is invalid returns an ERR_UNKNOWN_MESSAGE error code.
*/
int determine_open_response_error(char *status, char *response) {
    if (!strcmp(status, "ERR")) {
        strcpy(response, "Received an error message for the open command from the server\n");
        return 0;
    }

    if (!strcmp(status, "NLG")) {
        strcpy(response, "User is not logged in\n");
        return 0;
    }

    if (!strcmp(status, "NOK")) {
        strcpy(response, "Auction could not be started by the AS\n");
        return 0;
    }

    return ERR_UNKNOWN_ANSWER;
}


/**
* Makes a CLS request to the server. 
*
* Returns 0 and writes result to `response` if the equest is made and the server 
* responds with a valid message.

* Returns an error code if:
*   The request can't be made, either because the parameters are invalid or there is a
* connection error, timeout or underlying OS error. 
*   The server responds with an unknown protocol message.
*/
int handle_close (char *input, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG("Entered");
    if (!client->logged_in) 
        return ERR_NOT_LOGGED_IN;

    char *aid;
    /**
    * Validate user arguments
    */
    aid = strtok(input, " ");
    if (aid == NULL)
        return ERR_NULL_ARGS;

    if (!is_valid_aid(aid))
        return ERR_INVALID_AID;

    /**
    * Establish connection to server
    */
    int conn_fd;
    if ((conn_fd = open_tcp_connection_to_server(client)) < 0)
        return ERR_TCP_CONN_TO_SERVER;

    /**
    * Create and send the request
    * Format: CLS UID password aid 
    */
    char request[32] = {0};
    sprintf(request, "CLS %.6s %.8s %.3s\n", client->uid, client->passwd, aid);
    if (send_tcp_message(request, strlen(request), conn_fd) != 0) {
        close(conn_fd);
        if (errno == EPIPE)
            return ERR_TCP_CLOSED_CONN;

        return ERR_SENDING_TCP;
    }

    /**
    * Read and validate server response 
    * Format: CLS status 
    * eg: ROA OK 003 
    */

    // read the response command
    char command[8] = {0};
    int err = read_tcp_stream(command, 4, conn_fd);
    if (err) {
        close(conn_fd);
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return ERR_TIMEOUT_TCP;

        if (err == ERR_TCP_READ_CLOSED)
            return ERR_TCP_CLOSED_CONN;

        return ERR_RECEIVING_TCP;
    }

    // if command is not RCL, message is invalid
    if (strcmp(command, "RCL ") != 0) {
        close(conn_fd);
        return ERR_UNKNOWN_ANSWER;
    }

    // get RCL command status
    char status[4] = {0};
    err = read_tcp_stream(status, 3, conn_fd) != 0;
    if (err) {
        close(conn_fd);
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return ERR_TIMEOUT_TCP;

        if (err == ERR_TCP_READ_CLOSED)
            return ERR_TCP_CLOSED_CONN;

        return ERR_RECEIVING_TCP;
    }

    // if we don't receive OK status, check status and write result to response 
    if (strcmp(status, "OK\n") != 0) {
        // check for \n at the end of the message
        if (!is_lf_in_stream(conn_fd)) {
            close(conn_fd);
            return ERR_UNKNOWN_ANSWER;
        }

        close(conn_fd);
        return determine_close_response_error(status, response);
    }

    close(conn_fd);
    strcpy(response, "Auction sucessfully closed by the AS!\n");

    return 0;
}


/**
* Analyses a not OK response sent by the server to the CLS command.
* If it's a valid response returns 0 and writes corresponding result to `response`.
* If the message is invalid returns an ERR_UNKNOWN_MESSAGE error code.
*/
int determine_close_response_error(char *status, char *response) {
    if (!strcmp(status, "ERR")) {
        strcpy(response, "Received an error message for close command from the server\n");
        return 0;
    }

    if (!strcmp(status, "NLG")) {
        strcpy(response, "User is not logged in\n");
        return 0;
    }

    if (!strcmp(status, "EAU")) {
        strcpy(response, "Auction with given UID does not exist\n");
        return 0;
    }

    if (!strcmp(status, "EOW")) {
        strcpy(response, "Auction is not owned by the user\n");
        return 0;
    }

    if (!strcmp(status, "END")) {
        strcpy(response, "Auction has already ended\n");
        return 0;
    }

    // our extension of the protocol  :)
    if (!strcmp(status, "NOK")) {
        strcpy(response, "Auction could not be started by the AS\n");
        return 0;
    }

    return ERR_UNKNOWN_ANSWER;
}

int handle_show_asset (char *input, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG("Entered");
    char *aid;

    aid = strtok(input, " ");
    if (aid == NULL)
        return ERR_NULL_ARGS;

    if (!is_valid_aid(aid))
        return ERR_INVALID_AID;

    /**
    * Open connect to server with a 5s timeout socket
    */
    int conn_fd;
    if ((conn_fd = open_tcp_connection_to_server(client)) < 0)
        return ERR_TCP_CONN_TO_SERVER;

    /**
    * Create and send the request
    * Format: CLS UID password aid 
    */
    char request[16];
    sprintf(request, "SAS %.3s\n", aid);
    if (send_tcp_message(request, strlen(request), conn_fd) != 0) {
        close(conn_fd);
        if (errno == EPIPE)
            return ERR_TCP_CLOSED_CONN;

        return ERR_SENDING_TCP;
    }

    /**
    * Read and validate server response 
    * Format: RSA status [Fname Fsize Fdata]
    */
    char command[8] = {0};
    int err = read_tcp_stream(command, 4, conn_fd) != 0;
    if (err) {
        close(conn_fd);
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return ERR_TIMEOUT_TCP;

        if (err == ERR_TCP_READ_CLOSED)
            return ERR_TCP_CLOSED_CONN;

        return ERR_RECEIVING_TCP;
    }

    if (strcmp(command, "RSA ") != 0) {
        close(conn_fd);
        return ERR_UNKNOWN_ANSWER;
    }

    char status[4] = {0};
    if (read_tcp_stream(status, 3, conn_fd) != 0) {
        close(conn_fd);
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return ERR_TIMEOUT_TCP;

        if (err == ERR_TCP_READ_CLOSED)
            return ERR_TCP_CLOSED_CONN;

        return ERR_RECEIVING_TCP;
    }

    if (strcmp(status, "OK ") != 0) {
        if (!is_lf_in_stream(conn_fd)) {
            close(conn_fd);
            return ERR_UNKNOWN_ANSWER;
        }
        close(conn_fd);
        return determine_close_response_error(status, response);
    }

    /**
    * Read the Fname and Fsize from TCP stream.
    * We read byte by byte and after reading we check their validity.
    */
    char args[2][32];
    int argno = 0;
    char args_buffer[32];
    char *ptr = args_buffer;
    int curr_arg_len = 0;
    while (argno < 2) {
        ssize_t read = recv(conn_fd, ptr, 1, 0);
        if (read <= 0) {
            close(conn_fd);
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return ERR_TIMEOUT_TCP;

            return ERR_RECEIVING_TCP;
        }

        curr_arg_len += read;
        // argument is already too long for what we are expecting
        if (curr_arg_len - 1 > get_show_asset_arg_len(argno)) {
            close(conn_fd);
            return ERR_UNKNOWN_ANSWER;
        }

        if (*ptr == ' ') {
            *ptr = '\0';
            strcpy(args[argno], args_buffer);

            curr_arg_len = 0;
            argno++;
            ptr = args_buffer;
            continue;
        }

        ptr += read;
    }

    if (!is_valid_fname(args[0])) {
        close(conn_fd);
        return ERR_UNKNOWN_ANSWER;
    }

    if (!is_valid_fname(args[1])) {
        close(conn_fd);
        return ERR_UNKNOWN_ANSWER;
    }

    char *fname = args[0];
    int fsize = atoi(args[1]);

    if (fsize > 10000000) {
        close(conn_fd);
        return ERR_UNKNOWN_ANSWER;
    }

    /**
    * Create asset file
    */
    int asset_fd;
    if ((asset_fd = open(fname, O_CREAT | O_TRUNC | O_WRONLY, 0777)) < 0) {
        close(conn_fd);
        return ERR_CREAT_ASSET_FILE;
    }

    err = as_recv_asset_file(asset_fd, conn_fd, fsize);
    if (err) {
        close(asset_fd);
        close(conn_fd);
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return ERR_TIMEOUT_TCP;

        if (err == ERR_TCP_READ_CLOSED)
            return ERR_TCP_CLOSED_CONN;

        if (err == ERR_TCP_READ)
            return ERR_RECEIVING_TCP;

        if (err == ERR_INVALID_PROTOCOL)
            return ERR_UNKNOWN_ANSWER;
            
        if (err == ERR_WRITE_AF)
            return ERR_WRITE_ASSET_FILE;
    }

    close(conn_fd);
    sprintf(response, "Successfully retrieved asset %s of auction %.3s\n", fname, aid);
    return 0;
}


int get_show_asset_arg_len(int argno) {
    if (argno == 0)
        return FNAME_LEN;
    else if (argno == 1)
        return FSIZE_STR_LEN; 

    // this should never be reached
    return 0;
}


/**
* Rolls back the creation of an asset aborted mid download.
* It simply removes the file, it's a best effort rollback, aka if
* it doesn't work we don't care and the file stays there, corrupted :P 
**/
void rollback_asset_creation(char *fname) {
    remove(fname);
}


/**
 *  sends a message to the server asking to place a bid for auction
 * 
*/
int handle_bid (char *input, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG("Entered BID");
    if (!client->logged_in) {
        return ERR_NOT_LOGGED_IN;
    }

    char *aid, *value;

    aid = strtok(input, " ");
    if (aid == NULL)
        return ERR_NULL_ARGS;

    if (!is_valid_aid(aid))
        return ERR_INVALID_AID;

    value = strtok(NULL, "\n");
    if (value == NULL)
        return ERR_NULL_ARGS;

    if (!is_valid_start_value(value))
        return ERR_INVALID_BID_VAL;

     /**
    * Open connect to server with a 5s timeout socket
    */
    int conn_fd;
    if ((conn_fd = open_tcp_connection_to_server(client)) < 0)
        return ERR_TCP_CONN_TO_SERVER;

    /**
     * send request to server
    */
    int bid_value = atoi(value);
    char request[64];
    sprintf(request, "BID %.6s %.8s %.3s %d\n", client->uid, client->passwd, aid, bid_value);
    if (send_tcp_message(request, strlen(request), conn_fd) != 0) {
        close(conn_fd);
        if (errno == EPIPE)
            return ERR_TCP_CLOSED_CONN;

        return ERR_SENDING_TCP;
    }

    /**
     * Read and validate response from server 
    */
    char command[8] = {0};
    int err = read_tcp_stream(command, 4, conn_fd);
    if (err) {
        close(conn_fd);
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return ERR_TIMEOUT_TCP;

        if (err == ERR_TCP_READ_CLOSED)
            return ERR_TCP_CLOSED_CONN;

        return ERR_RECEIVING_TCP;
    }

    // if command is not RBD, message is invalid
    if (strcmp(command, "RBD ") != 0) {
        close(conn_fd);
        return ERR_UNKNOWN_ANSWER;
    }

    // get RBD command status
    char status[8] = {0};
    err = read_tcp_stream(status, 4, conn_fd);
    if (err) {
        close(conn_fd);
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return ERR_TIMEOUT_TCP;

        if (err == ERR_TCP_READ_CLOSED)
            return ERR_TCP_CLOSED_CONN;

        return ERR_RECEIVING_TCP;
    }

    if (strcmp(status, "ACC\n") != 0) {
        // check for \n at the end of the message
        close(conn_fd);
        return determine_bid_response_error(status, response);
    }

    sprintf(response, "Sucessfully bidded %s on auction %s\n", value, aid);
    close(conn_fd);
    return 0;
}

/**
* Analyses a response sent by the server to the BID command.
* If it's a valid response returns 0 and writes corresponding result to `response`.
* If the message is invalid returns an ERR_UNKNOWN_MESSAGE error code.
*/
int determine_bid_response_error(char *status, char *response) {
    if (!strcmp(status, "ERR\n")) {
        strcpy(response, "Received an error message for bid command from the server (auction might not exist or client parameters might be badly formatted!)\n");
        return 0;
    }

    if (!strcmp(status, "NLG\n")) {
        strcpy(response, "User is not logged in\n");
        return 0;
    }

    if (!strcmp(status, "ILG\n")) {
        strcpy(response, "User cannot bid on an auction hosted by himself\n");
        return 0;
    }

    if (!strcmp(status, "REF\n")) {
        strcpy(response, "The bid amound is too low\n");
        return 0;
    }

    if (!strcmp(status, "NOK\n")) {
        strcpy(response, "The auction is not active\n");
        return 0;
    }

    return ERR_UNKNOWN_ANSWER;
}

/**
* Creates a TCP socket with a 5 seconds timeout and connects it to the AS server.
* Returns the socket file descriptor on success and -1 on error.
*/
int open_tcp_connection_to_server(struct client_state *client) {
    int conn_fd;
    if ((conn_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        LOG_ERROR("Failed creating socket to communicate with server");
        LOG_DEBUG("socket: %s", strerror(errno));
        return -1;
    }

    struct timeval timeout;
    timeout.tv_sec = TCP_CLIENT_TIMEOUT; // 5 sec timeout
    timeout.tv_usec = 0;
    // set timeout for socket
    if (setsockopt(conn_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
        close(conn_fd);
        LOG_ERROR("Failed setting timeout for server connection");
        LOG_DEBUG("setsockopt: %s", strerror(errno));
        return -1;
    }

    if (connect(conn_fd, client->as_addr, (socklen_t)client->as_addr_len) != 0) {
        close(conn_fd);
        LOG_ERROR("Failed connecting to server");
        LOG_DEBUG("connect: %s", strerror(errno));
        return -1;
    }

    return conn_fd;
}
