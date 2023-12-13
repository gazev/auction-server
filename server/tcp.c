#include <asm-generic/errno.h>
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
#include "../utils/utils.h"

#include "server.h"
#include "tcp_command_table.h"
#include "database.h"
#include "tcp.h"

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
        LOG_ERROR("%s:%d - Failed setting time out socket for client", client->ipv4, client->port);
        LOG_ERROR("setsockopt: %s", strerror(errno));
        return -1;
    }

    /**
    * Read command message from stream
    */
    char cmd_buffer[8] = {0};
    int err = read_tcp_stream(cmd_buffer, 4, client->conn_fd);
    if (err) {
        // if couldn't receive message from client
        if (err == ERR_TCP_READ)
            LOG_VERBOSE("%s:%d - Failed receiving message from client", client->ipv4, client->port);
        
        if (err == ERR_TCP_READ_CLOSED)
            LOG_VERBOSE("%s:%d - Client closed connection", client->ipv4, client->port);
 
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            LOG_VERBOSE("%s:%d - Timed out client", client->ipv4, client->port);

       if (send_tcp_message("ERR\n", 4, client->conn_fd) != 0) {
            LOG_VERBOSE("%s:%d - Failed responding to client", client->ipv4, client->port);
            if (errno == EPIPE)
                LOG_VERBOSE("%s:%d - Client closed connection", client->ipv4, client->port);
        }

        if (close(client->conn_fd) != 0) {
            LOG_ERROR("Failed closing client connection, resources might be leaking");
            LOG_DEBUG("close: %s", strerror(errno));
        }

        return 0;
    }

    /**
    * Handle the command and if it fails, send ERR to the client
    */
    if (handle_tcp_command(cmd_buffer, client) != 0) {
        int err = send_tcp_message("ERR\n", 4, client->conn_fd);
        if (err) {
            LOG_VERBOSE("%s:%d - Failed responding to client", client->ipv4, client->port);
            if (errno == EPIPE)
                LOG_VERBOSE("%s:%d - Client closed connection", client->ipv4, client->port);
        }
    }

    if (close(client->conn_fd) != 0) {
        LOG_ERROR("Failed closing client->conn_fd, resources might be leaking");
        LOG_DEBUG("close: %s", strerror(errno));
    }

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
    /*
    * see tcp_command_table.h comment on tcp_handler_fn type definition to 
    * better understand how these handlers are expected to behave
    */
    tcp_handler_fn handler_fn = get_tcp_handler_fn(cmd);

    if (handler_fn == NULL) {
        LOG_VERBOSE("%s:%d - [TCP] Ignoring unknown command", client->ipv4, client->port);
        return -1;
    }

    /**
    * Handler will proccedd with the protocol communication if the command payload 
    * is well behaved, if not it will return an error code to be communicated to the client 
    */
    LOG_VERBOSE("%s:%d - [TCP] Handling %s command", client->ipv4, client->port, cmd);
    int handler_ret = handler_fn(client);
    if (handler_ret != 0) {
        LOG_VERBOSE("%s:%d - [TPC] Badly formatted command", client->ipv4, client->port);
        char *err_msg = get_tcp_error_msg(handler_ret);

        // send error message to client
        if (send_tcp_message(err_msg, strlen(err_msg), client->conn_fd) != 0) {
            LOG_VERBOSE("%s:%d - Failed responding to client", client->ipv4, client->port);
            if (errno == EPIPE)
                LOG_VERBOSE("%s:%d - Client closed connection", client->ipv4, client->port);
        }
    }

    return 0;
}

int handle_open(struct tcp_client *client) {
    LOG_DEBUG("%s:%d - [OPA] Entered handler", client->ipv4, client->port);
    char *uid, *passwd, *name, *fname;
    int sv, ta;
    long fsize;

    /**
    * Read request from TCP stream
    */
    char buff[128] = {0};
    // read uid and passwd
    int err = read_tcp_stream(buff, UID_SIZE + 1 + PASSWORD_SIZE + 1, client->conn_fd);
    if (err) { // time out or socket error
        LOG_VERBOSE("%s:%d - [OPA] Failed reading UID and password from TCP stream", client->ipv4, client->port);
        if (err == ERR_TCP_READ_CLOSED)
            LOG_VERBOSE("%s:%d - [OPA] Client closed connection", client->ipv4, client->port);

        if (err == ERR_TCP_READ)
            LOG_VERBOSE("%s:%d - [OPA] Failed reading from socket", client->ipv4, client->port);

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            LOG_VERBOSE("%s:%d - [OPA] Timed out client", client->ipv4, client->port);

        return OPA_BAD_ARGS;
    }

    // no delimiters after OPA and password
    if (buff[UID_SIZE + PASSWORD_SIZE + 1] != ' ') {
        LOG_VERBOSE("%s:%d - [OPA] Badly formatted command", client->ipv4, client->port);
        return OPA_BAD_ARGS;
    }

    uid = strtok(buff, " ");
    if (uid == NULL) {
        LOG_VERBOSE("%s:%d - [OPA] No UID", client->ipv4, client->port);
        return OPA_BAD_ARGS;
    }
    if (!is_valid_uid(uid)) {
        LOG_VERBOSE("%s:%d - [OPA] Invalid UID", client->ipv4, client->port);
        return OPA_BAD_ARGS;
    }

    passwd = strtok(NULL, " ");
    if (!is_valid_passwd(passwd)) {
        LOG_VERBOSE("%s:%d - [OPA] Invalid password", client->ipv4, client->port);
        return OPA_BAD_ARGS;
    }

    if (passwd == NULL) {
        LOG_VERBOSE("%s:%d - [OPA] No password", client->ipv4, client->port);
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
                LOG_VERBOSE("%s:%d - [OPA] Timed out client", client->ipv4, client->port);
            } else {
                LOG_VERBOSE("%s:%d - [OPA] Failed reading from socket", client->ipv4, client->port);
                LOG_DEBUG("%s:%d - recv: %s", client->ipv4, client->port, strerror(errno));
            }
            return OPA_BAD_ARGS;
        }

        if (read == 0) {
            LOG_VERBOSE("%s:%d - [OPA] Connection closed by client", client->ipv4, client->port);
            return 0;
        }

        curr_arg_len += read;
        if (curr_arg_len - 1 > get_opa_arg_len(argno)) {
            LOG_VERBOSE("%s:%d - [OPA] Invalid size for argumnet (argno: %d)", client->ipv4, client->port, argno);
            return OPA_BAD_ARGS;
        }

        // one argument received, read it 
        if (*ptr == ' ') {
            *ptr = '\0';
            // check if argument parsed is valid
            if (!is_valid_opa_arg(args_buff, argno)) {
                LOG_VERBOSE("%s:%d - [OPA] Invalid variable length argument (argno: %d)", client->ipv4, client->port, argno);
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

    name = args[0];
    sv = atoi(args[1]); // start value
    ta = atoi(args[2]); // time active
    fname = args[3];
    fsize = atol(args[4]);
    
    // this would lock an auction out of bid possibilities
    if (sv == 999999) {
        return OPA_BAD_ARGS;
    }

    // limit of 10 MB for an asset file
    if (fsize <= 0 || fsize > 10000000) {
        LOG_VERBOSE("%s:%d - [OPA] Invalid fsize", client->ipv4, client->port);
        return OPA_BAD_ARGS;
    }

    /**
    * Make database validations
    */
    if (!exists_user(uid) || !is_user_logged_in(uid)) {
        LOG_VERBOSE("%s:%d - [OPA] User %s doesn't exist or is not logged in", client->ipv4, client->port, uid);
        char *resp = "ROA NLG\n";
        if (send_tcp_message(resp, 8, client->conn_fd) != 0) {
            LOG_VERBOSE("%s:%d - [OPA] Failed responding ROA NLG", client->ipv4, client->port);
            if (errno == EPIPE)
                LOG_VERBOSE("%s:%d - Client closed connection", client->ipv4, client->port);
        }
        return 0;
    }

    if (!is_authentic_user(uid, passwd)) {
        LOG_VERBOSE("%s:%d - [OPA] Authentication failed for user %s", client->ipv4, client->port, uid);
        char *resp = "ROA NLG\n";
        if (send_tcp_message(resp, 8, client->conn_fd) != 0) {
            LOG_VERBOSE("%s:%d - [OPA] Failed responding ROA NLG", client->ipv4, client->port);
            if (errno == EPIPE)
                LOG_VERBOSE("%s:%d - [OPA] Client closed connection", client->ipv4, client->port);
        }
        return 0;
    }

    // create new auction
    int auction_id = create_new_auction(uid, name, fname, sv, ta, fsize, client->conn_fd);
    if (auction_id == -1) {
        LOG_VERBOSE("%s:%d - [OPA] Failed creating new auction", client->ipv4, client->port);

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            LOG_VERBOSE("%s:%d - [OPA] Timed out client", client->ipv4, client->port);

        char *resp = "ROA NOK\n";
        if (send_tcp_message(resp, 8, client->conn_fd) != 0) {
            LOG_VERBOSE("%s:%d - [OPA] Failed responding ROA NOK", client->ipv4, client->port);
            if (errno == EPIPE)
                LOG_VERBOSE("%s:%d - [OPA] Client closed connection", client->ipv4, client->port);
        }
        return 0;
    }

    /**
    * Respond to client
    */
    char resp[12];
    sprintf(resp, "ROA OK %03d\n", auction_id);
    if (send_tcp_message(resp, 11, client->conn_fd) != 0) {
        LOG_VERBOSE("%s:%d - [OPA] Failed responding ROA OK", client->ipv4, client->port);
        if (errno == EPIPE)
            LOG_VERBOSE("%s:%d - [OPA] Client closed connection", client->ipv4, client->port);
    }

    LOG_VERBOSE("%s:%d - [OPA] Auction %03d created for user %s", client->ipv4, client->port, auction_id, uid);

    return 0;
}

int handle_close(struct tcp_client *client) {
    LOG_DEBUG("%s:%d - [CLS] Entered handler", client->ipv4, client->port);

    char buff[128] = {0};
    int err = read_tcp_stream(buff, UID_SIZE + 1 + PASSWORD_SIZE + 1 + AID_SIZE + 1, client->conn_fd);
    if (err) {
        LOG_VERBOSE("%s:%d - [CLS] Failed reading UID and password from TCP stream", client->ipv4, client->port);
        if (err == ERR_TCP_READ_CLOSED)
            LOG_VERBOSE("%s:%d - [CLS] Client closed connection", client->ipv4, client->port);

        if (err == ERR_TCP_READ)
            LOG_VERBOSE("%s:%d - [CLS] Failed reading from socket", client->ipv4, client->port);

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            LOG_VERBOSE("%s:%d - [CLS] Timed out client", client->ipv4, client->port);

        return CLS_BAD_ARGS;
    }

    /**
    * Validate message format
    */
    if (buff[UID_SIZE + PASSWORD_SIZE + AID_SIZE + 2] != '\n') {
        LOG_VERBOSE("%s:%d - [CLS] Bad end token for command", client->ipv4, client->port);
        return CLS_BAD_ARGS;
    }

    /**
    * Validate message arguments
    */
    char *uid = strtok(buff, " "); 
    char *passwd = strtok(NULL, " ");
    char *aid = strtok(NULL, "\n");

    if (uid == NULL) {
        LOG_VERBOSE("%s:%d - [CLS] No UID", client->ipv4, client->port);
        return CLS_BAD_ARGS;
    }

    if (!is_valid_uid(uid)) {
        LOG_VERBOSE("%s:%d - [CLS] Invalid UID", client->ipv4, client->port);
        return CLS_BAD_ARGS;
    }

    if (passwd == NULL) {
        LOG_VERBOSE("%s:%d - [CLS] No passwordd", client->ipv4, client->port);
        return CLS_BAD_ARGS;
    }

    if (!is_valid_passwd(passwd)) {
        LOG_VERBOSE("%s:%d - [CLS] Invalid password", client->ipv4, client->port);
        return CLS_BAD_ARGS;
    }
 
    if (aid == NULL) {
        LOG_VERBOSE("%s:%d - [CLS] No AID", client->ipv4, client->port);
        return CLS_BAD_ARGS;
    }

    if (!is_valid_aid(aid)) {
        LOG_VERBOSE("%s:%d - [CLS] Invalid AID", client->ipv4, client->port);
        return CLS_BAD_ARGS;
    }

    // Update database /
    update_database();
    /*
    * Validate user and auction in database 
    */
    // check user against db
    if (!exists_user(uid) || !is_user_logged_in(uid) || !is_authentic_user(uid, passwd)) {
        LOG_VERBOSE("%s:%d - [CLS] User %s doesn't exist or is not logged in", client->ipv4, client->port, uid);
        char *resp = "RCL NLG\n";
        if (send_tcp_message(resp, 8, client->conn_fd) != 0) {
            LOG_VERBOSE("%s:%d - [CLS] Failed responding RCL NLG", client->ipv4, client->port);
            if (errno == EPIPE)
                LOG_VERBOSE("%s:%d - [CLS] Client closed connection", client->ipv4, client->port);
        }
        return 0;
    }

    // check if auction exists
    if (!exists_auction(aid)) {
        LOG_VERBOSE("%s:%d - [CLS] Auction %s doesn't exist", client->ipv4, client->port, aid);
        char *resp = "RCL EAU\n";
        if (send_tcp_message(resp, 8, client->conn_fd) != 0) {
            LOG_VERBOSE("%s:%d - [CLS] Failed responding RCL EAU", client->ipv4, client->port);
            if (errno == EPIPE)
                LOG_VERBOSE("%s:%d - [CLS] Client closed connection", client->ipv4, client->port);
        }
        return 0;
    }

    // read auction information 
    char auction_info[256];
    if (get_auction_info(aid, auction_info, 256) != 0) {
        LOG_VERBOSE("%s:%d - [CLS] Failed retrieving information about action %s", client->ipv4, client->port, aid);
        char *resp = "RCL NOK\n";
        if (send_tcp_message(resp, 8, client->conn_fd) != 0) {
            LOG_VERBOSE("%s:%d - [CLS] Failed responding RCL NOK", client->ipv4, client->port);
            if (errno == EPIPE)
                LOG_VERBOSE("%s:%d - [CLS] Client closed connection", client->ipv4, client->port);
        }
        return 0;
    }

    // check if user is the owner of desired auction
    char *owner_uid = strtok(auction_info, " ");
    if (strcmp(owner_uid, uid) != 0) {
        LOG_VERBOSE("%s:%d - [CLS] Invalid owner for auction %s", client->ipv4, client->port, aid);
        char *resp = "RCL EOW\n";
        if (send_tcp_message(resp, 8, client->conn_fd) != 0) {
            LOG_VERBOSE("%s:%d - [CLS] Failed responding RCL EOW", client->ipv4, client->port);
            if (errno == EPIPE)
                LOG_VERBOSE("%s:%d - [CLS] Client closed connection", client->ipv4, client->port);
        }
        return 0;
    }

    // check if auction is already finished
    if (is_auction_finished(aid)) {
        LOG_VERBOSE("%s:%d - [CLS] Auction %s has already finished", client->ipv4, client->port, aid);
        char *resp = "RCL END\n";
        if (send_tcp_message(resp, 8, client->conn_fd) != 0) {
            LOG_VERBOSE("%s:%d - [CLS] Failed responding RCL END", client->ipv4, client->port);
            if (errno == EPIPE)
                LOG_VERBOSE("%s:%d - [CLS] Client closed connection", client->ipv4, client->port);
        }
        return 0;
    }

    /**
    * Handle the CLS command 
    */
    if (close_auction(aid) != 0) {
        LOG_VERBOSE("%s:%d - [CLS] Auction %s couldn't be closed", client->ipv4, client->port, aid);
        char *resp = "RCL NOK\n";
        if (send_tcp_message(resp, 8, client->conn_fd) != 0) {
            LOG_VERBOSE("%s:%d - [CLS] Failed responding RCL END", client->ipv4, client->port);
            if (errno == EPIPE)
                LOG_VERBOSE("%s:%d - [CLS] Client closed connection", client->ipv4, client->port);
        }
        return 0;
    }

    // inform user of success
    char *resp = "RCL OK\n";
    if (send_tcp_message(resp, 7, client->conn_fd) != 0) {
        LOG_VERBOSE("%s:%d - [CLS] Failed responding RCL OK", client->ipv4, client->port);
        if (errno == EPIPE)
            LOG_VERBOSE("%s:%d - [CLS] Client closed connection", client->ipv4, client->port);
    }

    LOG_VERBOSE("%s:%d - [CLS] Closed auction %s for user %s", client->ipv4, client->port, aid, uid);

    return 0;
}

int handle_show_asset(struct tcp_client *client) {
    LOG_DEBUG("%s:%d - [SAS] Entered handler", client->ipv4, client->port);
    char *aid;

    /**
    * Read and validate command arguments
    */
    char buff[8] = {0}; // enough to fit AID + null terminating byte 
    int err = read_tcp_stream(buff, AID_SIZE + 1, client->conn_fd);
    if (err) {
        LOG_VERBOSE("%s:%d - [SAS] Failed reading UID and password from TCP stream", client->ipv4, client->port);
        if (err == ERR_TCP_READ_CLOSED)
            LOG_VERBOSE("%s:%d - Client closed connection", client->ipv4, client->port);

        if (err == ERR_TCP_READ)
            LOG_VERBOSE("%s:%d - Failed reading from socket", client->ipv4, client->port);

        if (errno == EAGAIN || errno == EWOULDBLOCK)
            LOG_VERBOSE("%s:%d - Timed out client", client->ipv4, client->port);

        return SAS_BAD_ARGS;
    }

    if (buff[AID_SIZE] != '\n') {
        LOG_VERBOSE("%s:%d - [SAS] Bad end token", client->ipv4, client->port);
        return SAS_BAD_ARGS;
    }

    aid = strtok(buff, "\n");
    if (aid == NULL) {
        LOG_VERBOSE("%s:%d - [SAS] No AID", client->ipv4, client->port);
        return SAS_BAD_ARGS;
    }

    if (!is_valid_aid(aid)) {
        LOG_VERBOSE("%s:%d - [SAS] Invalid AID", client->ipv4, client->port);
        return SAS_BAD_ARGS;
    }

    /**
    * Validate arguments with database
    */
    if (!exists_auction(aid)) {
        LOG_VERBOSE("%s:%d - [SAS] Auction doesn't exist", client->ipv4, client->port);
        char *resp = "RSA NOK\n";
        if (send_tcp_message(resp, 8, client->conn_fd) != 0) {
            LOG_VERBOSE("%s:%d - [SAS] Failed responding RSA NOK", client->ipv4, client->port);
            if (errno == EPIPE)
                LOG_VERBOSE("%s:%d - [SAS] Client closed connection", client->ipv4, client->port);
        }
        return 0;
    }

    /**
    * Retrieve asset information
    */
    char auction_info[256];
    if (get_auction_info(aid, auction_info, 256) != 0) {
        LOG_VERBOSE("%s:%d - [SAS] Failed retrieving %3s auction information", client->ipv4, client->port, aid);
        char *resp = "RSA NOK\n"; 
        if (send_tcp_message(resp, 8, client->conn_fd) != 0) {
            LOG_VERBOSE("%s:%d - [SAS] Failed responding RSA NOK", client->ipv4, client->port);
            if (errno == EPIPE)
                LOG_VERBOSE("%s:%d - [SAS] Client closed connection", client->ipv4, client->port);
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
        if (send_tcp_message(resp, 8, client->conn_fd) != 0) {
            LOG_VERBOSE("%s:%d - [SAS] Failed responding RSA NOK", client->ipv4, client->port);
            if (errno == EPIPE)
                LOG_VERBOSE("%s:%d - [SAS] Client closed connection", client->ipv4, client->port);
        }
        return 0;
    }

    struct stat st; 
    char asset_path[128];
    sprintf(asset_path, "AUCTIONS/%s/ASSET/%s", aid, asset_fname);
    if (stat(asset_path, &st) != 0) {
        LOG_DEBUG("%s:%d - [SAS] Failed retrieving %3s auction information", client->ipv4, client->port, aid);
        char *resp = "RSA NOK\n"; 
        if (send_tcp_message(resp, 8, client->conn_fd) != 0) {
            LOG_VERBOSE("%s:%d - [SAS] Failed responding RSA NOK", client->ipv4, client->port);
            if (errno == EPIPE)
                LOG_VERBOSE("%s:%d - [SAS] Client closed connection", client->ipv4, client->port);
        }
        return 0;
    }

    /**
    * Send response
    */
    int afd;
    if ((afd = open(asset_path, O_RDONLY, 0)) < 0) {
        LOG_DEBUG("%s:%d - [SAS] Failed retrieving %3s auction information", client->ipv4, client->port, aid);
        char *resp = "RSA NOK\n"; 
        if (send_tcp_message(resp, 8, client->conn_fd) != 0) {
            LOG_VERBOSE("%s:%d - [SAS] Failed responding RSA NOK", client->ipv4, client->port);
            if (errno == EPIPE)
                LOG_VERBOSE("%s:%d - [SAS] Client closed connection", client->ipv4, client->port);
        }
        return 0;
    }
    
    // send asset meta data
    char resp_info[128];
    long fsize = st.st_size;
    sprintf(resp_info, "RSA OK %s %ld ", asset_fname, st.st_size);
    if (send_tcp_message(resp_info, strlen(resp_info), client->conn_fd) != 0) {
        LOG_VERBOSE("%s:%d - [SAS] Failed sending asset information", client->ipv4, client->port);
        if (errno == EPIPE)
            LOG_VERBOSE("%s:%d - [SAS] Client closed connection", client->ipv4, client->port);
 
        close(afd);
        return 0;
    }

    if (as_send_asset_file(afd, client->conn_fd, fsize) < 0) {
        LOG_VERBOSE("%s:%d - [SAS] Failed sending asset file to client", client->ipv4, client->port);
        if (errno == EPIPE)
            LOG_VERBOSE("%s:%d - [SAS] Connection closed by client", client->ipv4, client->port);

        close(afd);
        return 0;
    }

    close(afd);

    LOG_VERBOSE("%s:%d - [SAS] Served asset %s", client->ipv4, client->port, aid);

    return 0;
}

int handle_bid(struct tcp_client *client) {
    LOG_DEBUG("%s:%d - [BID] Entered handler", client->ipv4, client->port);

    char buff[32] = {0};
    int err = read_tcp_stream(buff, UID_SIZE+1+PASSWORD_SIZE+1+AID_SIZE+1, client->conn_fd);
    if (err){
        LOG_VERBOSE("FAILED READING FROM TCP STREAM");
    }

    /* read bid value (byte by byte)*/
    char byte = '0'; // byte currently reading
    char value[MAX_BID_VALUE + 1] = {0}; // overall value
    int read = 0; // number of bytes read
    while (read < MAX_BID_VALUE) {
        ssize_t r = recv(client->conn_fd, &byte, 1, 0);
        if (r < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                LOG_VERBOSE("%s:%d - [BID] Timed out client", client->ipv4, client->port);
            } else {
                LOG_VERBOSE("%s:%d - [BID] Failed reading from socket", client->ipv4, client->port);
                LOG_ERROR("%s:%d - recv: %s", client->ipv4, client->port, strerror(errno));
            }
            return BID_BAD_ARGS;
        }

        if (r == 0) {
            LOG_VERBOSE("%s:%d - [BID] Connection closed by client", client->ipv4, client->port);
            return 0;
        }

        if (byte == '\n')
            break;

        //if the first digit is a 0 ignore
        if (read == 0 && byte=='0')
            continue;

        //if someone sent a letter in the middle
        if (byte < '0' || byte > '9') {
            LOG_VERBOSE("%s:%d - [BID] Invalid value", client->ipv4, client->port);
            return BID_BAD_ARGS;
        }

        value[read] = byte;
        read += 1;
    }

    int bid_value = atoi(value);

    /**
    * Validate message arguments
    */
    char *uid = strtok(buff, " "); 
    char *passwd = strtok(NULL, " ");
    char *aid = strtok(NULL, " ");

    if (uid == NULL) {
        LOG_VERBOSE("%s:%d - [BID] No UID", client->ipv4, client->port);
        return BID_BAD_ARGS;
    }

    if (!is_valid_uid(uid)) {
        LOG_VERBOSE("%s:%d - [BID] Invalid UID", client->ipv4, client->port);
        return BID_BAD_ARGS;
    }

    if (passwd == NULL) {
        LOG_VERBOSE("%s:%d - [BID] No password", client->ipv4, client->port);
        return BID_BAD_ARGS;
    }

    if (!is_valid_passwd(passwd)) {
        LOG_VERBOSE("%s:%d - [BID] Invalid password", client->ipv4, client->port);
        return BID_BAD_ARGS;
    }
 
    if (aid == NULL) {
        LOG_VERBOSE("%s:%d - [BID] No AID", client->ipv4, client->port);
        return BID_BAD_ARGS;
    }

    if (!is_valid_aid(aid)) {
        LOG_VERBOSE("%s:%d - [BID] Invalid AID", client->ipv4, client->port);
        return BID_BAD_ARGS;
    }

    // update database
    update_database();
    /**
    * Validate user and auction in database 
    */
    if (!exists_user(uid)) {
        LOG_VERBOSE("%s:%d - [BID] User %s doesn't exist", client->ipv4, client->port, uid);
        char *resp = "RBD NOK\n";
        if (send_tcp_message(resp, 8, client->conn_fd) != 0) {
            LOG_DEBUG("%s:%d - [BID] Failed send_tcp_response", client->ipv4, client->port);
        }
        return 0;
    }

    // check user against db
    if (!is_user_logged_in(uid) || !is_authentic_user(uid, passwd)) {
        LOG_VERBOSE("%s:%d - [BID] User %s is not logged in or failed authentication", client->ipv4, client->port, uid);
        char *resp = "RBD NLG\n";
        if (send_tcp_message(resp, 8, client->conn_fd) != 0) {
            LOG_DEBUG("%s:%d - [BID] Failed send_tcp_response", client->ipv4, client->port);
        }
        return 0;
    }

    // check if auction exists
    if (!exists_auction(aid)) {
        LOG_VERBOSE("%s:%d - [BID] Auction %s doesn't exist", client->ipv4, client->port, aid);
        char *resp = "RBD ERR\n";
        if (send_tcp_message(resp, 8, client->conn_fd) != 0) {
            LOG_DEBUG("%s:%d - [BID] Failed send_tcp_response", client->ipv4, client->port);
        }
        return 0;
    }

    // read auction information 
    char auction_info[256];
    if (get_auction_info(aid, auction_info, 256) != 0) {
        LOG_VERBOSE("%s:%d - [BID] Failed retrieving information about action %s", client->ipv4, client->port, aid);
        char *resp = "RBD NOK\n";
        if (send_tcp_message(resp, 8, client->conn_fd) != 0) {
            LOG_DEBUG("%s:%d - [BID] Failed send_tcp_response", client->ipv4, client->port);
        }
        return 0;
    }

    //"%s %s %s %d %d %s %ld\n", uid, name, fname, sv, ta, str_time, unix_start_time
    char *owner_uid = strtok(auction_info, " ");
    char *starting_value;
    for (int i=0; i<2; i++) //skip name and fname
        starting_value = strtok(NULL, " ");
    starting_value = strtok(NULL, " ");

    // check if user is the owner of desired auction
    if (strcmp(owner_uid, uid) == 0) {
        LOG_VERBOSE("%s:%d - [BID] Invalid owner for auction %s", client->ipv4, client->port, aid);
        char *resp = "RBD ILG\n";
        if (send_tcp_message(resp, 8, client->conn_fd) != 0) {
            LOG_DEBUG("%s:%d - [BID] Failed send_tcp_response", client->ipv4, client->port);
        }
        return 0;
    }

    // check if auction is already finished
    if (is_auction_finished(aid)) {
        LOG_VERBOSE("%s:%d - [BID] Auction %s has already finished", client->ipv4, client->port, aid);
        char *resp = "RBD NOK\n";
        if (send_tcp_message(resp, 8, client->conn_fd) != 0) {
            LOG_DEBUG("%s:%d - [BID] Failed send_tcp_response", client->ipv4, client->port);
        }
        return 0;
    }

    if (!is_valid_start_value(starting_value)){
        LOG_VERBOSE("%s:%d - [BID] Got a non numeric value from the starting value in file %s", client->ipv4, client->port, aid);
        return BID_BAD_ARGS;
    }

    int start_bid = atoi(starting_value);
    int last_bid = get_last_bid(aid);

    if (last_bid < 0) {
        LOG_VERBOSE("%s:%d - [BID] Failled getting last bid %s", client->ipv4, client->port, aid);
        return BID_BAD_ARGS;
    }

    if (last_bid == 0)
        last_bid = start_bid;

    //check if bid is acceptable
    if (bid_value <= last_bid) {
        char *resp = "RBD REF\n";
        if (send_tcp_message(resp, 8, client->conn_fd) != 0) {
            LOG_DEBUG("%s:%d - [BID] Failed send_tcp_response", client->ipv4, client->port);
        }
        return 0;
    }

    if (bid(aid, uid, bid_value)){
        LOG_VERBOSE("%s:%d - [RBD] Got a non numeric value from the starting value in file %s", client->ipv4, client->port, aid);
        return BID_BAD_ARGS;
    }

    LOG_VERBOSE("%s:%d - [BID] Successful bid %s", client->ipv4, client->port, aid);
    char *resp = "RBD ACC\n";
    if (send_tcp_message(resp, 8, client->conn_fd) != 0) {
        LOG_DEBUG("%s:%d - [BID] Failed send_tcp_response", client->ipv4, client->port);
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