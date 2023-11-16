#ifndef __COMMAND_ERRORS_H__
#define __COMMAND_ERRORS_H__

#define ERROR_LOGIN 1
#define ERROR_LOGOUT 2
#define ERROR_MYLIST 3

char *get_error_msg(int errcode);

#endif