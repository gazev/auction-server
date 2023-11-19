#ifndef __COMMAND_ERRORS_H__
#define __COMMAND_ERRORS_H__

#define ERROR_LOGIN 1
#define ERROR_LOGOUT 2
#define ERROR_MYLIST 3
#define ERROR_NULL_ARGS 4
#define ERROR_NULL_UID 5
#define ERROR_NULL_PASSWD 6
#define ERROR_INVALID_UID 7
#define ERROR_INVALID_PASSWD 8
#define ERROR_UNKNOWN_ANSWER 9

char *get_error_msg(int errcode);

#endif