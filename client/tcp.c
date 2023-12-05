#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include "../utils/logging.h"
#include "../utils/validators.h"

#include "client.h"
#include "command_table.h"
#include "tcp.h"

int open_tcp_connection_to_server(struct client_state *);
int read_tcp_stream(char *buff, size_t size, int conn_fd);
int send_tcp_message(char *buff, size_t size, int conn_fd);

int handle_open (char *input, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    if (!client->logged_in) 
        return ERR_NOT_LOGGED_IN;

    char *name, *asset_fname, *start_value, *time_active;

    /**
    * Validate client arguments
    */
    // validate name
    name = strtok(input, " ");
    if (name == NULL) {
        return ERR_NULL_ARGS;
    }

    if (!is_valid_name(name)) {
        return ERR_INVALID_ASSET_NAME;
    }

    // validate asset file name
    asset_fname = strtok(NULL, " ");
    if (asset_fname == NULL) {
        return ERR_NULL_ARGS;
    }

    if (!is_valid_fname(asset_fname)) {
        return ERR_INVALID_ASSET_FNAME;
    }

    // validate start value
    start_value = strtok(NULL, " ");
    if (start_value == NULL) {
        return ERR_NULL_ARGS;
    }

    if (!is_valid_start_value(start_value)) {
        return ERR_INVALID_SV;
    }

    // validate time active
    time_active = strtok(NULL, " ");
    if (time_active == NULL) {
        return ERR_NULL_ARGS;
    }

    if (!is_valid_time_active(time_active)) {
        return ERR_INVALID_TA;
    }

    /**
    * Get asset file information
    */
    struct stat st; 
    if (stat(asset_fname, &st) != 0) {
        return ERR_INVALID_ASSET_FILE;
    }

    // file is too large
    if (st.st_size > 10000000) {
        return ERR_INVALID_ASSET_FILE;
    }

    int asset_fd;
    // if file doesn't exist
    if ((asset_fd = open(asset_fname, O_RDONLY, 0)) < 0) {
        return ERR_INVALID_ASSET_FILE;
    }

    /**
    * Connect to server
    */
    int conn_fd;
    if ((conn_fd = open_tcp_connection_to_server(client)) < 0) {
        return ERR_TCP_CONN_TO_SERVER;
    }

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
        LOG_DEBUG("Failed sending asset meta info");
        return ERR_REQUESTING_TCP;
    }

    // send the actual asset data 
    int total_sent = 0;
    int sent;
    off_t offset = 0;
    while (total_sent < st.st_size) {
        if ((sent = sendfile(conn_fd, asset_fd, &offset, st.st_size - total_sent)) < 0) {
            close(asset_fd);
            close(conn_fd);
            LOG_DEBUG("Failed sending file to server");
            LOG_ERROR("sendfile: %s", strerror(errno));
            return ERR_REQUESTING_TCP;
        }

        total_sent += sent;
    }

    close(asset_fd);

    if (send_tcp_message("\n", 1, conn_fd) < 0) {
        close(asset_fd);
        close(conn_fd);
        LOG_DEBUG("Failed sending terminating TCP message token");
        return ERR_REQUESTING_TCP;
    }

    /**
    * Read and validate server response 
    * Format: ROA status [AID] 
    * eg: ROA OK 003 
    */
    char resp_cmd[8] = {0}; // enough to fit valid responses
    if (read_tcp_stream(resp_cmd, 4, conn_fd)) {
        close(conn_fd);
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return ERR_TIMEOUT_TCP;
        }
        return ERR_RECEIVING_TCP;
    }

    // if response command isn't ROA
    if (strcmp(resp_cmd, "ROA ")) {
        close(conn_fd);
        return ERR_UNKNOWN_ANSWER;
    }

    char resp_status[8]; // enough to fit status response
    if (read_tcp_stream(resp_status, 3, conn_fd)) {
        close(conn_fd);
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return ERR_TIMEOUT_TCP;
        }
        return ERR_RECEIVING_TCP;
    }

    // if an error occurred
    if (!strcmp(resp_status, "ERR")) {
        close(conn_fd);
        strcpy(response, "Received an error message from the server\n");
        return 0;
    }

    // this will only happen if the server, somehow, is faulty
    if (!strcmp(resp_status, "NLG")) {
        close(conn_fd);
        strcpy(response, "User is not logged in\n");
        return 0;
    }

    // auction coudln't be created
    if (!strcmp(resp_status, "NOK")) {
        close(conn_fd);
        strcpy(response, "Auction could not be started by the AS\n");
        return 0;
    }

    // if neither of the previous status was sent, then it can only be OK
    if (strcmp(resp_status, "OK ") != 0) {
        close(conn_fd);
        return ERR_UNKNOWN_ANSWER;
    }

    // read AID
    char aid_buff[8] = {0};
    if (read_tcp_stream(aid_buff, 4, conn_fd)) {
        close(conn_fd);
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return ERR_TIMEOUT_TCP;
        }
        return ERR_RECEIVING_TCP;
    }

    // validate received AID
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

int handle_close (char *input, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    if (!client->logged_in) 
        return ERR_NOT_LOGGED_IN;

    /**
    * Validate user arguments
    */
    char *aid = strtok(input, " ");
    if (aid == NULL) {
        return ERR_NULL_ARGS;
    }

    if (!is_valid_aid(aid)) {
        return ERR_INVALID_AID;
    }

    /**
    * Establish connection to server
    */
    int conn_fd;
    if ((conn_fd = open_tcp_connection_to_server(client)) < 0) {
        return ERR_TCP_CONN_TO_SERVER;
    }

    char request[32] = {0};
    sprintf(request, "CLS %.6s %.8s %.3s\n", client->uid, client->passwd, aid);
    if (send_tcp_message(request, strlen(request), conn_fd) != 0) {
        close(conn_fd);
        return ERR_REQUESTING_TCP;
    }

    char resp_command[8] = {0};
    if (read_tcp_stream(resp_command, 4, conn_fd) != 0) {
        close(conn_fd);
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return ERR_TIMEOUT_TCP;
        }
        return ERR_RECEIVING_TCP;
    }

    if (strcmp(resp_command, "RCL ") != 0) {
        close(conn_fd);
        return ERR_UNKNOWN_ANSWER;
    }

    char status[4] = {0};
    if (read_tcp_stream(resp_command, 3, conn_fd) != 0) {
        close(conn_fd);
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return ERR_TIMEOUT_TCP;
        }
        return ERR_RECEIVING_TCP;
    }

    if (!strcmp(status, "NLG")) {
        strcpy(response, "User is not logged in!");
        return 0;
    }

    if (!strcmp(status, "EAU")) {
        strcpy(response, "Auction with given AID doesn't exist!\n");
        return 0;
    }

    if (!strcmp(status, "EOW")) {
        strcpy(response, "This user does not own given auction\n");
        return 0;
    }

    if (!strcmp(status, "END")) {
        strcpy(response, "Auction has already eneded\n");
        return 0;
    }

    // only possible outcome after all checks    
    if (strcmp(status, "OK\n") != 0) {
        return ERR_UNKNOWN_ANSWER;
    }

    close(conn_fd);
    strcpy(response, "Auction sucessfully closed by the AS!\n");
    return 0;
}


int handle_show_asset (char *input, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG(" ");
    return 0;
}

int handle_bid (char *input, struct client_state *client, char response[MAX_SERVER_RESPONSE]) {
    LOG_DEBUG(" ");
    return 0;
}


/**
* Creates a TCP socket with a 5 seconds timeout and connects it to the AS server.
* Returns the socket file descriptor on success and -1 on error.
*/
int open_tcp_connection_to_server(struct client_state *client) {
    int conn_fd;
    if ((conn_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        LOG_DEBUG("Failed creating socket to communicate with server");
        LOG_ERROR("socket: %s", strerror(errno));
        return -1;
    }

    struct timeval timeout;
    timeout.tv_sec = 5; // 5 sec timeout
    timeout.tv_usec = 0;
    // set timeout for socket
    if (setsockopt(conn_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
        close(conn_fd);
        LOG_ERROR("Failed setting timeout for server connection");
        LOG_ERROR("setsockopt: %s", strerror(errno));
        return -1;
    }

    if (connect(conn_fd, client->as_addr, (socklen_t)client->as_addr_len) != 0) {
        close(conn_fd);
        LOG_DEBUG("Failed connecting to server");
        LOG_ERROR("connect: %s", strerror(errno));
        return -1;
    }

    return conn_fd;
}
/**
* Read from TCP stream into buffer with specified size. 
* Return zero on success and 1 on error. 
*/
int read_tcp_stream(char *buff, size_t size, int conn_fd) {
    char *ptr = buff;
    int total_read = 0;
    while (total_read < size) {
        ssize_t read = recv(conn_fd, ptr + total_read, size - total_read, 0);
        if (read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                LOG_DEBUG("Timed out receiving response from client");
                LOG_ERROR("recv: %s", strerror(errno));
            } else {
                LOG_ERROR("recv: %s", strerror(errno));
                LOG_DEBUG("Failed reading from socket");
            }
            return 1;
        }

        if (read == 0) {
            LOG_DEBUG("Connection closed by client");
            return 1;
        }

        total_read += read;
        ptr += read;
    }

    return 0;
}

/**
* Write passed msg of given size to TCP socket
* Return 0 on success and 1 on error
*/
int send_tcp_message(char *msg, size_t size, int conn_fd) {
    char *ptr = msg;
    int total_sent = 0;
    int sent;
    while (total_sent < size) {
        if ((sent = write(conn_fd, ptr, size - total_sent)) < 0) {
            LOG_DEBUG("Failed sending TCP message to server");
            LOG_ERROR("write: %s", strerror(errno));
            return 1;
        }

        total_sent += sent;
        ptr += sent;
    }

    return 0;
}
