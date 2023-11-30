#ifndef __DATABASE_H__
#define __DATABASE_H__

int init_database();
int exists_user(char *uid);

/**
* DB status API 
*/
int is_authentic_user(char *uid, char *passwd);
int is_user_logged_in(char *uid);

int exists_auction(char *aid);
int is_auction_finished(char *aid);
int get_auction_info(char *aid, char *buff, int n);
int get_auction_asset_filename(char *aid, char *buff, char n);

/**
* DB action API 
*/
int log_in_user(char *uid);
int log_out_user(char *uid);

int register_user(char *uid, char* passwd);
int unregister_user(char *uid);

int create_new_auction(char *uid, char *name, char *fname, int sv, int ta, int fisze, int fd);
int close_auction(char *aid);

#endif