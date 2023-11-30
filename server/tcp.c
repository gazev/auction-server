#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../utils/config.h"
#include "../utils/logging.h"
#include "../utils/validators.h"

#include "server.h"
#include "tcp_command_table.h"
#include "database.h"
#include "tcp.h"
#include "tcp_errors.h"

int read_tcp_stream(char *buff, int size, struct tcp_client *);
int send_tcp_response(char *buff, int size, struct tcp_client *);

int is_valid_opa_arg(char *arg, int argno);

/**
* Serve a TCP connection
*/
int serve_tcp_connection(struct tcp_client *client) {
    struct timeval timeout;
    timeout.tv_sec = 5; // 5 sec timeout
    timeout.tv_usec = 0;

    // set timeout for socket
    if (setsockopt(client->conn_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
        LOG_DEBUG("%s:%d - Failed setting time out socket for client", client->ipv4, client->port);
        LOG_ERROR("setsockopt: %s", strerror(errno));
        return -1;
    }

    /**
    * Read command message from stream
    */
    char cmd_buffer[8];
    char *ptr = cmd_buffer;
    int cmd_size = 3; // command size
    int total_read = 0;
    // read protocol operation 
    while (total_read < cmd_size) {
        ssize_t read = recv(client->conn_fd, ptr + total_read, cmd_size - total_read, 0);

        if (read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                LOG_VERBOSE("Timed out client %s:%d", client->ipv4, client->port);
                close(client->conn_fd);
                return 0;
            } else {
                LOG_ERROR("recv: %s", strerror(errno));
                LOG_DEBUG("Failed reading from socket. client %s:%d", client->ipv4, client->port);
                close(client->conn_fd);
                return -1;
            }
        }

        if (read == 0) {
            LOG_VERBOSE("Connection closed by client %s:%d", client->ipv4, client->port);
            close(client->conn_fd);
            return 0;
        }
        
        total_read += read;
        ptr += read;
    }

    // handle the command
    int err = handle_tcp_command(cmd_buffer, client);
    if (err) { // if we could not understand the command
        char *err_msg = "ERR\n";
        char *ptr = err_msg;
        char err_len = 4;
        int total_written = 0;
        while (total_written < err_len) {
            ssize_t written = send(client->conn_fd, ptr, err_len - total_written, 0);

            if (written < 0) {
                LOG_DEBUG("%s:%d - Failed responding to client", client->ipv4, client->port);
                LOG_ERROR("write: %s", strerror(errno));
                break;
            }

            total_written += written;
        }
    }

    close(client->conn_fd);

    return 0;
}

/**
* Handles command for a TCP client. 
* If 0 is returned, then the command was successfully handled and the client
* notified of the result.
*
* If -1 is returned, then the operation could not be handled and appropriate
* action should be taken to inform the client.
*/
int handle_tcp_command(char *cmd, struct tcp_client *client) {
    LOG_DEBUG("Entering handle_tcp_connection");

    /*
    * see tcp_command_table.h comment on tcp_handler_fn type definition to 
    * better understand how these handlers are expected to behave
    */
    tcp_handler_fn fn = get_tcp_handler_fn(cmd);

    if (fn == NULL) {
        LOG_VERBOSE("%s:%d - Ignoring unknown command", client->ipv4, client->port);
        return -1;
    }

    LOG_VERBOSE("Handling %s for %s:%d", cmd, client->ipv4, client->port);
    // handler will proccedd with the protocol communication if the command payload 
    // is well behaved, if not it will return an error code to be communicated to the client 
    int err = fn(cmd, client);
    if (err) {
        char *err_msg = get_tcp_error_msg(err);
        if (send_tcp_response(err_msg, 8, client) != 0) {
            LOG_DEBUG("%s:%d - Failed responding with error - %s - to client", client->ipv4, client->port, err_msg);
        }
    }

    return 0;
}

int handle_open(char *cmd, struct tcp_client *client) {
    // LOG_DEBUG("Entered handle_open")
    char buff[128];
    // read uid and passwd
    int err = read_tcp_stream(buff, UID_SIZE + PASSWORD_SIZE + 3, client);
    if (err) { // time out or socket error
        LOG_DEBUG("%s:%d - [OPA] Failed reading UID and password from TCP stream", client->ipv4, client->port);
        return 0;
    }

    // no delimiters after OPA and password
    if (buff[0] != ' ' || buff[UID_SIZE + PASSWORD_SIZE + 2] != ' ') {
        LOG_DEBUG("%s:%d - [OPA] Bad formatted command", client->ipv4, client->port);
        return OPA_BAD_ARGS;
    }

    char *uid = strtok(buff, " ");
    char *passwd = strtok(NULL, " ");

    if (uid == NULL) {
        LOG_DEBUG("%s:%d - [OPA] No UID", client->ipv4, client->port);
        return OPA_BAD_ARGS;
    }

    if (passwd == NULL) {
        LOG_DEBUG("%s:%d - [OPA] No password", client->ipv4, client->port);
        return OPA_BAD_ARGS;
    }

    if (!is_valid_uid(uid)) {
        LOG_DEBUG("%s:%d - [OPA] Invalid UID", client->ipv4, client->port);
        return OPA_BAD_ARGS;
    }

    if (!is_valid_passwd(passwd)) {
        LOG_DEBUG("%s:%d - [OPA] Invalid password", client->ipv4, client->port);
        return OPA_BAD_ARGS;
    }

    /**
    * We will read byte by byte because it is not trivial how to read variable sized
    * tokens without any prior knowledge on them and we have limited time ;(
    */
    char args_buff[128];
    char args[5][30]; // name, start_value, timeactive, fname, fsize
    int argno = 0; // count
    int curr_arg_len = 0;
    char *ptr = args_buff;
    while (argno < 5) {
        ssize_t read = recv(client->conn_fd, ptr, 1, 0);
        if (read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                LOG_VERBOSE("%s:%d - Timed out client", client->ipv4, client->port);
            } else {
                LOG_DEBUG("%s:%d - [OPA] Failed reading from socket", client->ipv4, client->port);
                LOG_ERROR("%s:%d - [OPA] recv: %s", client->ipv4, client->port, strerror(errno));
            }
            return 0;
        }

        if (read == 0) {
            LOG_VERBOSE("%s:%d - Connection closed by client", client->ipv4, client->port);
            return 0;
        }

        curr_arg_len += read;
        if (curr_arg_len - 1 > get_opa_arg_len(argno)) {
            LOG_DEBUG("%s:%d - [OPA] Invalid size for argumnet (argno: %d)", client->ipv4, client->port, argno);
            return OPA_BAD_ARGS;
        }

        // one argument received, read it 
        if (*ptr == ' ') {
            *ptr = '\0';
            // check if argument parsed is valid
            if (!is_valid_opa_arg(args_buff, argno)) {
                LOG_DEBUG("%s:%d - [OPA] Invalid variable length argument (argno: %d)", client->ipv4, client->port, argno);
                return OPA_BAD_ARGS;
            }
            // save arg
            strcpy(args[argno], args_buff);
            ptr = args_buff;

            curr_arg_len = 0;
            argno++;
            continue;
        }

        ptr += read;
    }

    char *name = args[0];
    int sv = atoi(args[1]); // start value
    int ta = atoi(args[2]); // time active

    char *fname = args[3];
    long fsize = atol(args[4]);
    
    if (fsize <= 0 || fsize > 10000000) {
        LOG_DEBUG("%s:%d - [OPA] Invalid fsize", client->ipv4, client->port);
        return OPA_BAD_ARGS;
    }

    if (!exists_user(uid) || !is_user_logged_in(uid)) {
        LOG_DEBUG("%s:%d - [OPA] User %s doesn't exist or is not logged in", client->ipv4, client->port, uid);
        char *resp = "ROA NLG\n";
        if (send_tcp_response(resp, 8, client) != 0) {
            LOG_DEBUG("%s:%d - [OPA] Failed responding ROA NLG", client->ipv4, client->port);
        }
        return 0;
    }

    if (!is_authentic_user(uid, passwd)) {
        LOG_DEBUG("%s:%d - [OPA] Authentication failed for user %s", client->ipv4, client->port, uid);
        char *resp = "ROA NLG\n";
        if (send_tcp_response(resp, 8, client) != 0) {
            LOG_DEBUG("%s:%d - [OPA] Failed sending ROA NLG response", client->ipv4, client->port);
        }
        return 0;
    }

    // create new auction
    int auction_id = create_new_auction(uid, name, fname, sv, ta, fsize, client->conn_fd);
    if (auction_id == -1) {
        LOG_DEBUG("%s:%d - [OPA] Failed creating new auction", client->ipv4, client->port);
        char *resp = "ROA NOK\n";
        if (send_tcp_response(resp, 8, client) != 0) {
            LOG_DEBUG("%s:%d - [OPA] Failed sending ROA NOK response", client->ipv4, client->port);
        }
        return 0;
    }

    // respond to client
    char resp[12];
    sprintf(resp, "ROA OK %03d\n", auction_id);
    if (send_tcp_response(resp, 11, client) != 0) {
        LOG_DEBUG("%s:%d - [OPA] Failed sending ROA OK response", client->ipv4, client->port);
    }

    LOG_VERBOSE("%s:%d - Auction %03d created for user %s", client->ipv4, client->port, auction_id, uid);

    return 0;
}

int handle_close(char *cmd, struct tcp_client *client) {
    LOG_DEBUG("%s:%d - Entered handle_close", client->ipv4, client->port);

    char buff[UID_SIZE + PASSWORD_SIZE + AID_SIZE + 4];
    int err = read_tcp_stream(buff, UID_SIZE + PASSWORD_SIZE + AID_SIZE + 4, client);
    if (err) {
        LOG_DEBUG("%s:%d - Failed reading from TCP stream", client->ipv4, client->port);
        return 0;
    }

    /**
    * Validate message format
    */
    if (buff[0] != ' ' || buff[UID_SIZE + PASSWORD_SIZE + AID_SIZE + 3] != '\n') {
        LOG_DEBUG("%s:%d - Bad formmated CLS command", client->ipv4, client->port);
        return CLS_BAD_ARGS;
    }

    /**
    * Validate message arguments
    */
    char *uid = strtok(buff, " "); 
    char *passwd = strtok(NULL, " ");
    char *aid = strtok(NULL, "\n");

    if (uid == NULL) {
        LOG_DEBUG("%s:%d - [CLS] No UID", client->ipv4, client->port);
        return CLS_BAD_ARGS;
    }

    if (!is_valid_uid(uid)) {
        LOG_DEBUG("%s:%d - [CLS] Invalid UID", client->ipv4, client->port);
        return CLS_BAD_ARGS;
    }

    if (passwd == NULL) {
        LOG_DEBUG("%s:%d - [CLS] No passwordd", client->ipv4, client->port);
        return CLS_BAD_ARGS;
    }

    if (!is_valid_passwd(passwd)) {
        LOG_DEBUG("%s:%d - [CLS] Invalid password", client->ipv4, client->port);
        return CLS_BAD_ARGS;
    }
 
    if (aid == NULL) {
        LOG_DEBUG("%s:%d - [CLS] No AID", client->ipv4, client->port);
        return CLS_BAD_ARGS;
    }

    if (!is_valid_aid(aid)) {
        LOG_DEBUG("%s:%d - [CLS] Invalid AID", client->ipv4, client->port);
        return CLS_BAD_ARGS;
    }

    /**
    * Validate user and auction in database 
    */
    // check user against db
    if (!exists_user(uid) || !is_user_logged_in(uid) || !is_authentic_user(uid, passwd)) {
        LOG_DEBUG("%s:%d - [CLS] User %s doesn't exist or is not logged in", client->ipv4, client->port, uid);
        char *resp = "RCL NLG\n";
        if (send_tcp_response(resp, 8, client) != 0) {
            LOG_DEBUG("%s:%d - [CLS] Failed send_tcp_response", client->ipv4, client->port);
        }
        return 0;
    }

    // check if auction exists
    if (!exists_auction(aid)) {
        LOG_DEBUG("%s:%d - [CLS] Auction %s doesn't exist", client->ipv4, client->port, aid);
        char *resp = "RCL EAU\n";
        if (send_tcp_response(resp, 8, client) != 0) {
            LOG_DEBUG("%s:%d - [CLS] Failed send_tcp_response", client->ipv4, client->port);
        }
        return 0;
    }

    // read auction information 
    char auction_info[256];
    if (get_auction_info(aid, auction_info, 256) != 0) {
        LOG_DEBUG("%s:%d - [CLS] Failed retrieving information about action %s", client->ipv4, client->port, aid);
        char *resp = "RCL NOK\n";
        if (send_tcp_response(resp, 8, client) != 0) {
            LOG_DEBUG("%s:%d - [CLS] Failed send_tcp_response", client->ipv4, client->port);
        }
        return 0;
    }

    // check if user is the owner of desired auction
    char *owner_uid = strtok(auction_info, " ");
    if (strcmp(owner_uid, uid) != 0) {
        LOG_DEBUG("%s:%d - [CLS] Auction %s doesn't exist", client->ipv4, client->port, aid);
        char *resp = "RCL EOW\n";
        if (send_tcp_response(resp, 8, client) != 0) {
            LOG_DEBUG("%s:%d - [CLS] Failed send_tcp_response", client->ipv4, client->port);
        }
        return 0;
    }

    // check if auction is already finished
    if (is_auction_finished(aid)) {
        LOG_DEBUG("%s:%d - [CLS] Auction %s has already finished", client->ipv4, client->port, aid);
        char *resp = "RCL END\n";
        if (send_tcp_response(resp, 8, client) != 0) {
            LOG_DEBUG("%s:%d - [CLS] Failed send_tcp_response", client->ipv4, client->port);
        }
        return 0;
    }

    /**
    * Handle the CLS command 
    */
    if (close_auction(aid) != 0) {
        LOG_DEBUG("%s:%d - [CLS] Auction %s couldn't be closed", client->ipv4, client->port, aid);
        char *resp = "RCL NOK\n";
        if (send_tcp_response(resp, 8, client) != 0) {
            LOG_DEBUG("%s:%d - [CLS] Failed send_tcp_response", client->ipv4, client->port);
        }
        return 0;
    }

    // inform user of success
    char *resp = "RCL OK\n";
    if (send_tcp_response(resp, 11, client) != 0) {
        LOG_DEBUG("%s:%d - [CLS] Failed send_tcp_response", client->ipv4, client->port);
    }

    LOG_VERBOSE("%s:%d - Closed auction %s for user %s", client->ipv4, client->port, aid, uid);

    return 0;
}

int handle_show_asset(char *cmd, struct tcp_client *client) {
    LOG_DEBUG("Entered handle_show_asset");

    char buff[AID_SIZE + 2];
    int err = read_tcp_stream(buff, AID_SIZE + 2, client);
    if (err) {
        LOG_DEBUG("%s:%d - [SAS] Failed reading from TCP stream", client->ipv4, client->port);
        return 0;
    }

    if (buff[0] != ' ' || buff[AID_SIZE + 1] != '\n') {
        LOG_DEBUG("%s:%d - [SAS] Bad formatted SAS command", client->ipv4, client->port);
        return SAS_BAD_ARGS;
    }

    char *aid = strtok(buff + 1, "\n"); // ignore white space

    if (aid == NULL) {
        LOG_DEBUG("%s:%d - [SAS] No AID", client->ipv4, client->port);
        return SAS_BAD_ARGS;
    }

    if (!is_valid_aid(aid)) {
        LOG_DEBUG("%s:%d - [SAS] Invalid AID", client->ipv4, client->port);
        return SAS_BAD_ARGS;
    }

    if (!exists_auction(aid)) {
        LOG_DEBUG("%s:%d - [SAS] Auction doesn't exist", client->ipv4, client->port);
        char *resp = "RSA NOK\n";
        if (send_tcp_response(resp, 8, client) != 0) {
            LOG_DEBUG("%s:%d - [SAS] Failed sending RSA NOK response", client->ipv4, client->port);
        }
        return 0;
    }

    char auction_info[256];
    if (get_auction_info(aid, auction_info, 256) != 0) {
        LOG_DEBUG("%s:%d - [SAS] Failed retrieving %3s auction information", client->ipv4, client->port, aid);
        char *resp = "RSA NOK\n"; 
        if (send_tcp_response(resp, 8, client) != 0) {
            LOG_DEBUG("%s:%d - [SAS] Failed sending RSA NOK response", client->ipv4, client->port);
        }
        return 0;
    }

    // read asset file path
    char *asset_fname = strtok(auction_info, " ");
    for (int i = 0; i < 2; ++i) {
        asset_fname = strtok(NULL, " ");
    }

    if (asset_fname == NULL) {
        LOG_DEBUG("%s:%d - [SAS] Failed retrieving %3s auction information", client->ipv4, client->port, aid);
        char *resp = "RSA NOK\n"; 
        if (send_tcp_response(resp, 8, client) != 0) {
            LOG_DEBUG("%s:%d - [SAS] Failed sending RSA NOK response", client->ipv4, client->port);
        }
        return 0;
    }

    int afd;
    char asset_path[128];
    sprintf(asset_path, "AUCTIONS/%3s/%s", aid, asset_fname);
    if ((afd = open(asset_path, O_RDONLY, 0)) < 0) {
        LOG_DEBUG("%s:%d - [SAS] Failed retrieving %3s auction information", client->ipv4, client->port, aid);
        char *resp = "RSA NOK\n"; 
        if (send_tcp_response(resp, 8, client) != 0) {
            LOG_DEBUG("%s:%d - [SAS] Failed sending RSA NOK response", client->ipv4, client->port);
        }
        return 0;
    }

    /**
    * Get file size
    */
    struct stat st; 
    if (stat(asset_path, &st) != 0) {
        close(afd);
        LOG_DEBUG("%s:%d - [SAS] Failed retrieving %3s auction information", client->ipv4, client->port, aid);
        char *resp = "RSA NOK\n"; 
        if (send_tcp_response(resp, 8, client) != 0) {
            LOG_DEBUG("%s:%d - [SAS] Failed sending RSA NOK response", client->ipv4, client->port);
        }
        return 0;
    }

    long size = st.st_size;

    /**
    * Send response
    */
    // send info
    char resp_info[128];
    sprintf(resp_info, "RSA OK %s %ld ", asset_fname, size);
    if (send_tcp_response(resp_info, strlen(resp_info), client) != 0) {
        close(afd);
        LOG_DEBUG("%s:%d - [SAS] Failed sending RSA OK response", client->ipv4, client->port);
        return 0;
    }

    // send file
    if (sendfile(client->conn_fd, afd, 0, size) != 0) {
        LOG_DEBUG("%s:%d - [SAS] Failed RSA sending asset file", client->ipv4, client->port);
    }

    close(afd);

    LOG_VERBOSE("%s:%d - Served asset %s", client->ipv4, client->port, aid);

    return 0;
}

int handle_bid(char *cmd, struct tcp_client *client) {
    LOG_DEBUG("%s:%d - [BID] Entered handle_bid", client->ipv4, client->port);
    return 0;
}

/**
* Read from TCP stream into buffer with specified size. On success, returns number of
* read bytes, on failure returns -1 
*/
int read_tcp_stream(char *buff, int size, struct tcp_client *client) {
    char *ptr = buff;
    int total_read = 0;
    while (total_read < size) {
        ssize_t read = recv(client->conn_fd, ptr + total_read, size - total_read, 0);
        if (read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                LOG_VERBOSE("Timed out client %s:%d", client->ipv4, client->port);
            } else {
                LOG_ERROR("recv: %s", strerror(errno));
                LOG_DEBUG("%s:%d - Failed reading from socket", client->ipv4, client->port);
            }
            return 1;
        }

        if (read == 0) {
            LOG_VERBOSE("Connection closed by client %s:%d", client->ipv4, client->port);
            return 1;
        }

        total_read += read;
        ptr += read;
    }

    return 0;
}

int send_tcp_response(char *resp, int size, struct tcp_client *client) {
    char *ptr = resp;
    int total_sent = 0;
    while (total_sent < size) {
        ssize_t written = send(client->conn_fd, ptr + total_sent, size - total_sent, 0);
        if (written < 0) {
            if (errno == EPIPE) {
                LOG_VERBOSE("Connection closed by client %s:%d", client->ipv4, client->port);
            } else {
                LOG_ERROR("send: %s", strerror(errno));
                LOG_DEBUG("%s:%d - Failed sending response to client", client->ipv4, client->port);
            }
            return -1;
        }

        total_sent += written;
        ptr += written;
    }

    return 0;
}

int is_valid_opa_arg(char *arg, int argno) {
    switch (argno) {
        case 0: return is_valid_name(arg);
        case 1: return is_valid_start_value(arg);
        case 2: return is_valid_time_active(arg);
        case 3: return is_valid_fname(arg);
        case 4: return is_valid_fsize(arg);
        default: return -1;
    }
}