#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

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
* Handles command for a TCP client. 
* If 0 is returned, then the command was successfully handled and the client
* notified of the result.
*
* If -1 is returned, then the operation could not be handled and appropriate
* action should be taken to inform the client.
*/
int handle_tcp_command(char *cmd, struct tcp_client *client) {
    LOG_DEBUG("entering handle_tcp_connection");

    /*
    * see tcp_command_table.h comment on tcp_handler_fn type definition to 
    * better understand how these handlers are expected to behave
    */
    tcp_handler_fn fn = get_tcp_handler_fn(cmd);

    if (fn == NULL) {
        LOG_DEBUG("NULL tcp handler function");
        return -1;
    }

    // handler will proccedd with the protocol communication if the command payload 
    // is well behaved, if not it will return an error code to be communicated to the client 
    int err = fn(cmd, client);
    if (err) {
        char *err_msg = get_tcp_error_msg(err);
        if (send_tcp_response(err_msg, 8, client) != 0) {
            LOG_DEBUG("failed responding with errmessage to client %s", client->ipv4);
        }
    }

    return 0;
}

int handle_open(char *cmd, struct tcp_client *client) {
    LOG_DEBUG("entered handle_open")

    char buff[1024];
    // read uid and passwd
    int err = read_tcp_stream(buff, UID_SIZE + PASSWORD_SIZE + 3, client);
    if (err) {
        LOG_DEBUG("failed reading uid and passwd from stream");
        return OPA_BAD_ARGS;
    }

    // no delimiters after OPA and password
    if (buff[0] != ' ' || buff[UID_SIZE + PASSWORD_SIZE + 2] != ' ') {
        LOG_DEBUG("bad formatted OPA command");
        return OPA_BAD_ARGS;
    }

    char *uid = strtok(buff, " ");
    char *passwd = strtok(NULL, " ");

    if (uid == NULL) {
        LOG_DEBUG("no uid");
        return OPA_BAD_ARGS;
    }

    if (passwd == NULL) {
        LOG_DEBUG("no password");
        return OPA_BAD_ARGS;
    }

    if (!is_valid_uid(uid)) {
        LOG_DEBUG("invalid uid");
        return OPA_BAD_ARGS;
    }

    if (!is_valid_passwd(passwd)) {
        LOG_DEBUG("invalid password");
        return OPA_BAD_ARGS;
    }

    /**
    * We will read byte by byte because it is not trivial how to read variable sized
    * tokens without any prior knowledge on them and we have limited time ;(
    */
    char args[5][30]; // name, start_value, timeactive, fname, fsize
    int argno = 0; // count
    int curr_arg_len = 0;
    char *ptr = buff;
    while (argno < 5) {
        int read = recv(client->conn_fd, ptr, 1, 0);
        if (read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                LOG_VERBOSE("timed out client %s", client->ipv4);
            } else {
                LOG_ERROR("recv: %s", strerror(errno));
                LOG_DEBUG("failed reading from socket. client %s", client->ipv4);
            }
            return 0;
        }

        if (read == 0) {
            LOG_VERBOSE("connection closed by client");
            return 0;
        }

        curr_arg_len += read;
        if (curr_arg_len - 1 > get_opa_arg_len(argno)) {
            LOG_DEBUG("invalid size for argumnet (argno: %d)", argno);
            return OPA_BAD_ARGS;
        }

        // one argument received, read it 
        if (*ptr == ' ') {
            *ptr = '\0';
            // check if argument parsed is valid
            if (!is_valid_opa_arg(buff, argno)) {
                LOG_VERBOSE("invalid variable length argument (argno: %d) in OPA", argno);
                return -1;
            }
            // save arg
            strcpy(args[argno], buff);
            ptr = buff;

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
    long fsize = atol(args[3]);
    
    if (fsize < 0 || fsize > 10000000) {
        LOG_DEBUG("invalid fsize");
        return OPA_BAD_ARGS;
    }

    if (!exists_user(uid) || !is_user_logged_in(uid)) {
        LOG_DEBUG("user doesn't exist or is not logged in")
        char *resp = "ROA NLG\n";
        if (send_tcp_response(resp, 8, client) != 0) {
            LOG_DEBUG("failed send_tcp_response");
        }
        return 0;
    }

    if (!is_authentic_user(uid, passwd)) {
        LOG_DEBUG("failed authentication for user %s", uid);
        char *resp = "ROA NLG\n";
        if (send_tcp_response(resp, 8, client) != 0) {
            LOG_DEBUG("failed send_tcp_response");
        }
        return 0;
    }

    // create new auction
    int auction_id = create_new_auction(uid, name, fname, sv, ta, fsize, client->conn_fd);
    if (auction_id == -1) {
        LOG_DEBUG("failed creating new auction")
        char *resp = "ROA NOK\n";
        if (send_tcp_response(resp, 8, client) != 0) {
            LOG_DEBUG("failed send_tcp_response");
        }
        return 0;
    }

    LOG_VERBOSE("auction %03d sucessfully created for user %s", auction_id, uid);

    // respond to client
    char resp[12];
    sprintf(resp, "ROA OK %03d\n", auction_id);
    if (send_tcp_response(resp, 11, client) != 0) {
        LOG_DEBUG("failed send_tcp_response");
    }

    return 0;
}

int handle_close(char *cmd, struct tcp_client *client) {
    LOG_DEBUG("entered handle_close")
    char buff[1024];

    int err = read_tcp_stream(buff, UID_SIZE + PASSWORD_SIZE + AID_SIZE + 4, client);
    if (err) {
        LOG_DEBUG("failed reading from tcp stream");
        return OPA_BAD_ARGS;
    }

    if (buff[0] != ' ' || buff[UID_SIZE + PASSWORD_SIZE + AID_SIZE + 3] != '\n') {
        LOG_DEBUG("invalid format");
        return OPA_BAD_ARGS;
    }

    char *uid = strtok(buff, " "); 
    char *passwd = strtok(NULL, " ");
    char *aid = strtok(NULL, " ");

    if (uid == NULL) {
        LOG_DEBUG("no uid");
        return -1;
    }

    if (passwd == NULL) {
        LOG_DEBUG("no passwd");
        return -1;
    }

    if (aid == NULL) {
        LOG_DEBUG("no aid");
        return -1;
    }

    if (!is_valid_uid(uid)) {
        LOG_DEBUG("invalid uid");
        return -1;
    }

    if (!is_valid_passwd(passwd)) {
        LOG_DEBUG("invalid passwd");
        return -1;
    }

    if (!is_valid_aid(aid)) {
        LOG_DEBUG("invalid aid");
        return -1;
    }

    return 0;
}

int handle_show_asset(char *cmd, struct tcp_client *client) {
    LOG_DEBUG("entered handle_show_asset");
    char buff[1024];

    int err = read_tcp_stream(buff, AID_SIZE + 2, client);
    if (err) {
        LOG_DEBUG("failed reading from tcp stream");
        return -1;
    }

    if (buff[0] != ' ' || buff[AID_SIZE + 1] != '\n') {
        LOG_DEBUG("invalid format");
        return -1;
    }

    char *aid = strtok(buff, " ");

    if (aid == NULL) {
        return -1;
    }

    if (!is_valid_aid(aid)) {
        return -1;
    }

    return 0;
}

int handle_bid(char *cmd, struct tcp_client *client) {
    LOG_DEBUG("entered handle_bid")
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
        int read = recv(client->conn_fd, ptr + total_read, size - total_read, 0);
        if (read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                LOG_VERBOSE("timed out client %s", client->ipv4);
            } else {
                LOG_ERROR("recv: %s", strerror(errno));
                LOG_DEBUG("failed reading from socket. client %s", client->ipv4);
            }
            return -1;
        }

        if (read == 0) {
            LOG_VERBOSE("connection closed by client %s", client->ipv4);
            return -1;
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
        int written = send(client->conn_fd, ptr + total_sent, size - total_sent, 0);
        if (written < 0) {
            if (errno == EPIPE) {
                LOG_DEBUG("connection closed by client %s", client->ipv4);
            } else {
                LOG_ERROR("send: %s", strerror(errno));
                LOG_DEBUG("failed sending response to client %s", client->ipv4);
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