#ifndef __COMMAND_TABLE_H__
#define __COMMAND_TABLE_H__

#include "../utils/constants.h"

#include "client.h"

#define ERR_TIMEDOUT_UDP 1
#define ERR_REQUESTING_UDP 2
#define ERR_RECEIVING_UDP 3

#define ERR_NULL_ARGS 4
#define ERR_INVALID_UID 5
#define ERR_INVALID_PASSWD 6
#define ERR_INVALID_AID 7
#define ERR_UNKNOWN_ANSWER 8
#define ERR_FILE_DOESNT_EXIST 9
#define ERR_INVALID_ASSET_NAME 10
#define ERR_INVALID_ASSET_FNAME 11
#define ERR_INVALID_SV 12
#define ERR_INVALID_TA 13
#define ERR_INVALID_ASSET_FILE 14

#define ERR_TCP_CONN_TO_SERVER 15
#define ERR_TIMEOUT_TCP 16 
#define ERR_RECEIVING_TCP 17 
#define ERR_SENDING_TCP 18 

#define ERR_NOT_LOGGED_IN 19
#define ERR_ALREADY_LOGGED_IN 20
#define ERR_NOT_LOGGED_OUT 21
#define ERR_NO_COMMAND 22 
#define ERR_INVALID_COMMAND 23

#define ERR_CREAT_ASSET_FILE 24
#define ERR_WRITE_ASSET_FILE 25
#define ERR_READ_ASSET_FILE 26

#define ERR_INVALID_BID_VAL 27

#define ERR_TCP_CLOSED_CONN 28

typedef int (*handler_func)(char *, struct client_state *, char *);

handler_func get_handler_func(char *cmd_op);
char *get_error_msg(int errcode);

#endif 