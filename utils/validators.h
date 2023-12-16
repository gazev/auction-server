#ifndef __VALIDATORS_H__
#define __VALIDATORS_H__

int is_valid_port(char *);
int is_valid_ip_addr(char *);

int is_valid_uid(char *);
int is_valid_passwd(char *);
int is_valid_aid(char *);

int is_valid_name(char *);
int is_valid_start_value(char *);
int is_valid_time_active(char *);

int is_valid_fname(char *);
int is_valid_fsize(char *);

int is_valid_date_time(char *date, char *time);

#endif