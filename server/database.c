#include <asm-generic/errno-base.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <dirent.h>

#include <sys/stat.h>

#include "../utils/config.h"

#include "database.h"

const static mode_t SERVER_MODE = S_IREAD | S_IWRITE | S_IEXEC;

/**
* Initializes DB. Returns 0 on success and -1 on fatal error.
*/
int init_database() {
    // LOG_DEBUG("entered init_database");
    if (mkdir(DB_ROOT, SERVER_MODE) != 0) {
        if (errno != EEXIST) {
            // LOG_ERROR("mkdir: %s", strerror(errno));
            // LOG_DEBUG("failed creating root directory, fatal...");
            return -1;;
        }
    }

    if (chdir(DB_ROOT) != 0) {
        // LOG_ERROR("chdir: %s", strerror(errno));
        return -1;
    }

    if (mkdir("USERS", SERVER_MODE) != 0) {
        if (errno != EEXIST) {
            // LOG_ERROR("mkdir: %s", strerror(errno));
            // LOG_DEBUG("failed creating USERS directory, fatal...");
            return -1;
        }
    }

    if (mkdir("AUCTIONS", SERVER_MODE) != 0) {
        if (errno != EEXIST) {
            // LOG_ERROR("mkdir: %s", strerror(errno));
            // LOG_DEBUG("failed creating AUCTIONS directory, fatal...");
            return -1;
        }
    }

    return 0;
}

/**
* Check if user is registred in DB 
*/
int exists_user(char *uid) {
    char user_passwd_path[256];
    FILE *fp;

    // check password file
    sprintf(user_passwd_path, "USERS/%s/%s_pass.txt", uid, uid);
    if ((fp = fopen(user_passwd_path, "r")) == NULL) {
        return 0;
    }

    fclose(fp);

    return 1;
}

/**
* Checks if user is logged in
*/
int is_user_logged_in(char *uid) {
    char user_login_path[256];
    FILE *fp;

    sprintf(user_login_path, "USERS/%s/%s_login.txt", uid, uid);
    if ((fp = fopen(user_login_path, "r")) == NULL) {
        return 0;
    }

    fclose(fp);

    return 1;
}

/**
* Logs in the user with uid. Returns 0 on success and -1 on failure
*/
int log_in_user(char *uid) {
    char user_login_path[256];
    FILE *fp;

    sprintf(user_login_path, "USERS/%s/%s_login.txt", uid, uid);
    if ((fp = fopen(user_login_path, "r")) == NULL) {
        return -1;
    }

    fclose(fp);

    return 0;
}

/**
* Creates a user in the DB. Returns 0 on success and -1 on failure
*/
int register_user(char *uid, char *passwd) {
    char user_file_path[256];
    
    // create user directory (e.g root/USERS/123456)
    sprintf(user_file_path, "USERS/%s", uid);
    if (mkdir(user_file_path, SERVER_MODE) != 0) {
        if (errno != EEXIST) {
            perror("mkdir");
            return -1;
        }
    }
    
    // create user password file (e.g root/USERS/123456/123456_pass.txt)
    sprintf(user_file_path, "USERS/%s/%s_pass.txt", uid, uid);
    int pass_fd = open(user_file_path, O_CREAT | O_WRONLY, SERVER_MODE);
    if (pass_fd == -1) {
        perror("open");
        return -1;
    }

    // write password
    if (write(pass_fd, passwd, 8) != 8) {
        perror("write");
        return -1;
    }

    // mark user as logged in (e.g touch root/USERS/123456/123456_login.txt)
    sprintf(user_file_path, "USERS/%s/%s_login.txt", uid, uid);
    if (open(user_file_path, O_CREAT, SERVER_MODE) == -1) {
        perror("open");
        return -1;
    }

    // create user's HOSTED dir
    sprintf(user_file_path, "USERS/%s/HOSTED", uid);
    if (mkdir(user_file_path, SERVER_MODE) != 0) {
        if (errno != EEXIST) {
            perror("mkdir");
            return -1;
        }
    }
    
    // create user's BIDDED dir
    sprintf(user_file_path, "USERS/%s/BIDDED", uid);
    if (mkdir(user_file_path, SERVER_MODE) != 0) {
        if (errno != EEXIST) {
            perror("mkdir");
            return -1;
        }
    }
 
    return 0;
}

/**
* Unregister a user.
*/
int unregister_user(char *uid) {
    char user_file_path[256];

    // remove user's passwd
    sprintf(user_file_path, "USERS/%s/%s_pass.txt", uid, uid);
    if (remove(user_file_path) != 0) {
        perror("remove");
        return -1;
    }

    // remove user's login
    sprintf(user_file_path, "USERS/%s/%s_login.txt", uid, uid);
    if (remove(user_file_path) != 0) {
        perror("remove");
        return -1;
    }

    DIR *dp;
    struct dirent *cur;
    char asset_file_path[256];

    // remove all assets in HOSTED
    sprintf(user_file_path, "USERS/%s/HOSTED", uid);
    if ((dp = opendir(user_file_path)) == NULL) {
        perror("opendir");
        return -1;
    }

    while ((cur = readdir(dp)) != NULL) {
        if (cur->d_name[0] == '.') continue;

        sprintf(asset_file_path, "USERS/%s/HOSTED/%s", uid, cur->d_name);
        if (remove(asset_file_path) != 0) {
            perror("remove");
        }
    }

    // remove all assets in BIDDED
    sprintf(user_file_path, "USERS/%s/BIDDED", uid);
    if ((dp = opendir(user_file_path)) == NULL) {
        perror("opendir");
        return -1;
    }

    while ((cur = readdir(dp)) != NULL) {
        if (cur->d_name[0] == '.') continue; // ignore hidden files

        sprintf(asset_file_path, "USERS/%s/BIDDED/%s", uid, cur->d_name);
        if (remove(asset_file_path) != 0) {
            perror("remove");
        }
    }

    return 0;
}

/**
* Returns 1 if provided password matches the password stored for user with uid
*/
int is_authentic_user(char *uid, char *passwd) {
    FILE *fp;
    char passwd_path_file[256];
    char stored_password[9];

    sprintf(passwd_path_file, "USERS/%s/%s_pass.txt", uid, uid);
    if ((fp = fopen(passwd_path_file, "r")) == NULL) {
        return 0;
    }

    fgets(stored_password, 9, fp);


    return strcmp(stored_password, passwd) == 0 ? 1 : 0;
}
