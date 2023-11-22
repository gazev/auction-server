#ifndef __DATABASE_H__
#define __DATABASE_H__

int init_database();
int exists_user(char *uid);
int is_user_logged_in(char *uid);
int log_in_user(char *uid);
int log_out_user(char *uid);

int register_user(char *uid, char* passwd);
int unregsiter_user(char *uid);

int is_authentic_user(char *uid, char *passwd);

#endif