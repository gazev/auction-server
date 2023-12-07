#include <stdlib.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include "config.h"

/**
Check if port string is a valid port number 
*/
int is_valid_port(char *port) {
    int port_int = atoi(port);

    // atoi returns 0 on error (not a numeric string), 
    // then we also check lower and upper limits of port numbers ]0, 65536[
    return !(port_int <= 0 || port_int >= 65536);
}


/**
Check if ip string is a valid IPv4 address 
*/
int is_valid_ip_addr(char *ip_str) {
    struct sockaddr_in sa;

    // inet_pton does a lot of things, but we are just interested in the fact that
    // it returns 0 if the string is not an IPv4 address and 1 if it is.
    return inet_pton(AF_INET, ip_str, &(sa.sin_addr));
}


/**
 * Check if the uid is valid
*/
int is_valid_uid(char *uid){
    if (strlen(uid) != UID_SIZE)
        return 0;

    for (int i = 0; i < UID_SIZE; ++i) {
        if(!isdigit(uid[i])) return 0;
    }

    return 1;
}


/**
 * Check if the password is valid
*/
int is_valid_passwd(char *passwd){
    if (strlen(passwd) != PASSWORD_SIZE)
        return 0;

    for (int i = 0; i < PASSWORD_SIZE; i++) {
        if (!isalnum(passwd[i])) return 0;
    }

    return 1;
}


int is_valid_name_char(char c) {
    return isalnum(c) ||
           // special characters
           c == '-' || c == '@' || 
           c == '!' || c == '?';
}

/**
* Check if given name for an asset is valid
*/
int is_valid_name(char *name) {
    size_t len = strlen(name);

    if (len > ASSET_NAME_LEN)
        return 0;

    for (int i = 0; i < len; i++)
        if (!is_valid_name_char(name[i])) return 0;

    return 1;
}


/**
* Check if given start value string is a valid start value (simply numeric with len <= 6)
*/
int is_valid_start_value(char *sv) {
    size_t len = strlen(sv);

    if (len > START_VALUE_LEN || len == 0)
        return 0;

    for (int i = 0; i < len; i++)
        if (!isdigit(sv[i])) return 0;
    
    return 1;
}


/**
* Check if given time active string is a valid time active value (numeric with len <= 5)
*/
int is_valid_time_active(char *ta) {
    size_t len = strlen(ta);

    if (len > TIME_ACTIVE_LEN || len == 0)
        return 0;

    for (int i = 0; i < len; i++)
        if (!isdigit(ta[i])) return 0;

    return 1;
}


/**
* We do not allow creation of files with these characters, this is subjective, 
* apart from '/' which is mandatory
*/
int is_valid_path_char(char c) {
    return c != '/' && 
            c != ':' && c != '*' && c != '\\' && 
            c != '`' && c != '~' &&
            c != '|' && c != '?' && c != '!' &&
            c != '#' && c != '$' && c != '%';
}

/** 
* Check if given fname is valid
*/
int is_valid_fname(char *fname) {
    size_t len = strlen(fname);

    if (len > FNAME_LEN || len == 0)
        return 0;

    // don't let people upload hidden files
    if (fname[0] == '.')
        return 0;

    for (int i = 0; i < len; i++)
        if (!is_valid_path_char(fname[i])) return 0;

    return 1;
}


/**
* Check if given fsize is valid
*/
int is_valid_fsize(char *fsize) {
    size_t len = strlen(fsize);

    if (len > FSIZE_STR_LEN || len == 0)
        return 0;

    for (int i = 0; i < len; i++)
        if (!isdigit(fsize[i])) return 0;
    
    return 1;
}


int is_valid_aid(char *aid) {
    size_t len = strlen(aid);

    if (len != AID_SIZE)
        return 0;

    for (int i = 0; i < len; i++)
        if (!isdigit(aid[i])) return 0;

    return 1;
}


/**
* Check if given date and time strings match the format YYYY-MM-DD HH:MM:SS
*/
int is_valid_date_time(char *date, char *time) {
        /**
    * Validate datetime string
    */
    char date_time_str[64];
    strcpy(date_time_str, date);
    strcat(date_time_str, " ");
    strcat(date_time_str, time);
     
    struct tm tm;
    if (sscanf(date_time_str, "%4d-%2d-%2d %2d:%2d:%2d",
                            &tm.tm_year, &tm.tm_mon, &tm.tm_mday,
                            &tm.tm_hour, &tm.tm_min, &tm.tm_sec) != 6)
        return 0;

    // TODO check if this works in lab
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;

    // convert from tm struct to string back and see if it matches original
    char reconverted_date_time_str[64] = {0};
    if (strftime(reconverted_date_time_str, sizeof(date_time_str), "%Y-%m-%d %H:%M:%S", &tm) != 19)
        return 0;

    return strcmp(date_time_str, reconverted_date_time_str) == 0;
}
