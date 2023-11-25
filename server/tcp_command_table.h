#ifndef __TCP_COMMAND_TABLE_H__
#define __TCP_COMMAND_TABLE_H__

/** 
* Handler function for TCP commands. 
*
* These functions should return an error value if the command payload was not 
* well behaved, e.g, incorrect syntax or invalid parameters.
* 
* If the command payload is valid, the handler function should perform the 
* issued command and complete the protocol communication with the client.
*/
typedef int (*tcp_handler_fn)(char *cmd, struct tcp_client *);

tcp_handler_fn get_tcp_handler_fn(char *cmd);
char *get_tcp_error_msg(int errcode);

int get_opa_arg_len(int);

#endif