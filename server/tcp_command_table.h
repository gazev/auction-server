#ifndef __TCP_COMMAND_TABLE_H__
#define __TCP_COMMAND_TABLE_H__

#define OPA_BAD_ARGS 1
#define CLS_BAD_ARGS 2
#define SAS_BAD_ARGS 3
#define BID_BAD_ARGS 4

/** 
* Handler function for TCP commands. 
*
* These functions should return an error value if the command payload was not 
* well behaved, e.g, incorrect syntax or invalid parameters.
* 
* If the command payload is valid, the handler function should perform the 
* issued command and complete the protocol communication with the client.
*/
typedef int (*tcp_handler_fn)(struct tcp_client *);

tcp_handler_fn get_tcp_handler_fn(char *cmd);
char *get_tcp_error_msg(int errcode);

int get_opa_arg_len(int);

#endif