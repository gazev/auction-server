#ifndef __DATABASE_H__
#define __DATABASE_H__

int init_database();
int update_database();

/**
* DB status API 
*/
int exists_user(char *uid);
int is_authentic_user(char *uid, char *passwd);
int is_user_logged_in(char *uid);

int exists_auction(char *aid);
int is_auction_finished(char *aid);

int get_auction_info(char *aid, char *buff, int n);
int get_user_auctions(char *uid, char *buff);
int get_auctions_list(char *buff);

int get_last_bid(char *aid);

/**
* DB action API 
*/
int log_in_user(char *uid);
int log_out_user(char *uid);

int register_user(char *uid, char* passwd);
int unregister_user(char *uid);

int create_new_auction(char *uid, char *name, char *fname, int sv, int ta, int fisze, int fd);
int close_auction(char *aid);

int bid(char *aid, char *uid, int value);

#endif