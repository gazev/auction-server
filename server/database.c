#include <asm-generic/errno-base.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <dirent.h>
#include <time.h>

#include <sys/stat.h>

#include "../utils/config.h"
#include "../utils/logging.h"

#include "database.h"


const static mode_t SERVER_MODE = S_IREAD | S_IWRITE | S_IEXEC;

static int auc_count = 0;
void load_db_state();

/**
* Initializes DB. Returns 0 on success and -1 on fatal error.
*/
int init_database() {
    LOG_DEBUG("entered init_database");
    if (mkdir(DB_ROOT, SERVER_MODE) != 0) {
        if (errno != EEXIST) {
            LOG_ERROR("mkdir: %s", strerror(errno));
            LOG_DEBUG("failed creating root directory, fatal...");
            return -1;;
        }
    }

    if (chdir(DB_ROOT) != 0) {
        LOG_ERROR("chdir: %s", strerror(errno));
        return -1;
    }

    if (mkdir("USERS", SERVER_MODE) != 0) {
        if (errno != EEXIST) {
            LOG_ERROR("mkdir: %s", strerror(errno));
            LOG_DEBUG("failed creating USERS directory, fatal...");
            return -1;
        }
    }

    if (mkdir("AUCTIONS", SERVER_MODE) != 0) {
        if (errno != EEXIST) {
            LOG_ERROR("mkdir: %s", strerror(errno));
            LOG_DEBUG("failed creating AUCTIONS directory, fatal...");
            return -1;
        }
    }

    if (open("db_state.txt", O_CREAT | O_WRONLY, SERVER_MODE) < 0) {
        LOG_ERROR("open: %s", strerror(errno));
        LOG_DEBUG("failed creating db_state.txt file");
        return -1;
    }

    load_db_state();

    return 0;
}

void load_db_state() {
    char bid_c[4];

    FILE *fp = fopen("db_state.txt", "r");
    fgets(bid_c, 4, fp);

    // no state
    if (strlen(bid_c) == 0) {
        return;
    }

    auc_count = atoi(bid_c);
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

    sprintf(user_login_path, "USERS/%s/%s_login.txt", uid, uid);
    if (open(user_login_path, O_CREAT, SERVER_MODE) == -1) {
        LOG_DEBUG("couldn't create login file for user %s", uid);
        return -1;
    }

    return 0;
}

int log_out_user(char *uid) {
    char user_login_path[256];

    sprintf(user_login_path, "USERS/%s/%s_login.txt", uid, uid);
    if (remove(user_login_path) != 0) {
        LOG_ERROR("remove: %s", strerror(errno));
        LOG_DEBUG("failed removing login file %s", uid);
        return -1;
    }

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
            LOG_ERROR("mkdir: %s", strerror(errno));
            LOG_DEBUG("couldn't  create user directory for user %s", uid);
            return -1;
        }
    }
    
    // create user's HOSTED dir
    sprintf(user_file_path, "USERS/%s/HOSTED", uid);
    if (mkdir(user_file_path, SERVER_MODE) != 0) {
        if (errno != EEXIST) {
            LOG_ERROR("mkdir: %s", strerror(errno));
            LOG_DEBUG("couldn't create HOSTED directory for user %s", uid);
            return -1;
        }
    }
    
    // create user's BIDDED dir
    sprintf(user_file_path, "USERS/%s/BIDDED", uid);
    if (mkdir(user_file_path, SERVER_MODE) != 0) {
        if (errno != EEXIST) {
            LOG_ERROR("mkdir: %s", strerror(errno));
            LOG_DEBUG("couldn't create BIDDED directory for user %s", uid);
            return -1;
        }
    }

   // mark user as logged in (e.g touch root/USERS/123456/123456_login.txt)
    sprintf(user_file_path, "USERS/%s/%s_login.txt", uid, uid);
    if (open(user_file_path, O_CREAT, SERVER_MODE) == -1) {
        LOG_ERROR("open: %s", strerror(errno));
        LOG_DEBUG("couldn't create login file for user %s", uid);
        return -1;
    }

    // create user password file (e.g root/USERS/123456/123456_pass.txt)
    sprintf(user_file_path, "USERS/%s/%s_pass.txt", uid, uid);
    int pass_fd = open(user_file_path, O_CREAT | O_WRONLY, SERVER_MODE);
    if (pass_fd == -1) {
        LOG_ERROR("open: %s", strerror(errno));
        LOG_DEBUG("couldn't create user password file for user %s", uid);
        return -1;
    }

    // write password
    if (write(pass_fd, passwd, 8) != 8) {
        LOG_ERROR("write: %s", strerror(errno));
        LOG_DEBUG("couldn't write password for user %s", uid);
        if (remove(user_file_path) != 0) {
            LOG_DEBUG("failed removing %s_pass.txt after failure registering user, ghost user %s is now created", uid, uid);
        }
        return -1;
    }
 
    return 0;
}

/**
* Unregister a user. Returns 0 on success and -1 on failure, which may screw the
* user's bids and hosted auctions. It would take way too much work to use tmp files for this, also,
* databases are so complex to implement we simply assume it's not necessary for 
* this project
*/
int unregister_user(char *uid) {
    char user_file_path[256];

    // remove user's login
    sprintf(user_file_path, "USERS/%s/%s_login.txt", uid, uid);
    if (remove(user_file_path) != 0) {
        LOG_ERROR("remove: %s", strerror(errno));
        LOG_DEBUG("failed removing login file for user %s", uid);
        return -1;
    }

    DIR *dp;
    struct dirent *cur;
    char asset_file_path[256];

    // remove all assets in HOSTED
    sprintf(user_file_path, "USERS/%s/HOSTED", uid);
    if ((dp = opendir(user_file_path)) == NULL) {
        LOG_ERROR("open: %s", strerror(errno));
        LOG_DEBUG("failed openning HOSTED directory for user %s", uid);
        return -1;
    }

    while ((cur = readdir(dp)) != NULL) {
        if (cur->d_name[0] == '.') continue;

        sprintf(asset_file_path, "USERS/%s/HOSTED/%s", uid, cur->d_name);
        if (remove(asset_file_path) != 0) {
            LOG_ERROR("remove: %s", strerror(errno));
            LOG_DEBUG("couldn't remove asset file %s", asset_file_path);
        }
    }

    // remove all assets in BIDDED
    sprintf(user_file_path, "USERS/%s/BIDDED", uid);
    if ((dp = opendir(user_file_path)) == NULL) {
        LOG_ERROR("open: %s", strerror(errno));
        LOG_DEBUG("failed openning BIDDED directory for user %s", uid);
        return -1;
    }

    while ((cur = readdir(dp)) != NULL) {
        if (cur->d_name[0] == '.') continue; // ignore hidden files

        sprintf(asset_file_path, "USERS/%s/BIDDED/%s", uid, cur->d_name);
        if (remove(asset_file_path) != 0) {
            LOG_ERROR("remove: %s", strerror(errno));
            LOG_DEBUG("couldn't remove asset file %s", asset_file_path);
        }
    }

    // remove user's passwd
    sprintf(user_file_path, "USERS/%s/%s_pass.txt", uid, uid);
    if (remove(user_file_path) != 0) {
        LOG_ERROR("remove: %s", uid);
        LOG_DEBUG("failed removing password file for user %s", uid);
        return -1;
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

/**
* Creates a new auction and returns it's AID if successfull. On failure returns -1
*/
int create_auction_dir() { 
    char auction_file_path[256];
    char bids_file_path[256];

    // if we reached the limit auctions
    if (auc_count >= 999) {
        return -1;
    }

    auc_count++;

    // create auction directory    
    sprintf(auction_file_path, "AUCTIONS/%03d", auc_count);
    if (mkdir(auction_file_path, SERVER_MODE) != 0) {
        if (errno != EEXIST) {
            LOG_ERROR("mkdir: %s", strerror(errno));
            LOG_DEBUG("failed creating new auction %s", auction_file_path);
            return -1;
        }
    }

    // create BIDS folder inside dir
    sprintf(bids_file_path, "AUCTIONS/%03d/BIDS", auc_count);
    if (mkdir(auction_file_path, SERVER_MODE) != 0) {
        if (errno != EEXIST) {
            LOG_ERROR("mkdir: %s", strerror(errno));
            LOG_DEBUG("failed creating bids folder for auction %s", bids_file_path);
            return -1;
        }
    }

    return auc_count;
}

void rollback_auction_dir_creation() {
    char auction_file_path[256];
    char bids_file_path[256];

    DIR *dp;
    struct dirent *cur;

    // directory for auction doesn't exist (creation failed because max limit was exceeded)
    sprintf(auction_file_path, "AUCTIONS/%03d", auc_count);
    if ((dp = opendir(auction_file_path)) == NULL) {
        if (errno == ENOENT) { // directory doesn't exist
            LOG_DEBUG("doesn't exist / wasn't created");
        } else {
            LOG_ERROR("open: %s", strerror(errno));
            LOG_DEBUG("failed opening %03d auction directory on rollback action, database might be corrupted", auc_count);
        }
        return;
    }

    char asset_file[256];
    // remove all files inside directory
    while ((cur = readdir(dp)) != NULL) {
        if (cur->d_name[0] == '.') continue;

        // remove all files
        if (cur->d_type == DT_REG) {
            sprintf(asset_file, "AUCTIONS/%03d/%s", auc_count, cur->d_name);
            if (remove(asset_file) != 0) {
                LOG_ERROR("remove: %s", strerror(errno));
                LOG_DEBUG("couldn't remove asset file %s on rollback action, database might be corrupted", cur->d_name);
            }
        }
    }

    // remove bids if they exist
    sprintf(bids_file_path, "AUCTIONS/%03d/BIDS", auc_count);
    if ((dp = opendir(bids_file_path)) != NULL) {
        while ((cur = readdir(dp)) != NULL) {
            if (cur->d_name[0] == '.') continue;

            if (cur->d_type == DT_REG) {
                sprintf(asset_file, "AUCTIONS/%03d/BIDS/%s", auc_count, cur->d_name);
                if (remove(asset_file) != 0) {
                    LOG_ERROR("remove: %s", strerror(errno));
                    LOG_DEBUG("couldn't remove bid file %s on rollback action, database might be corrupted", cur->d_name);
                }
            }

        }
    }

    // remove directory
    if ((remove(auction_file_path) != 0)) {
        LOG_ERROR("remove: %s", strerror(errno));
        LOG_DEBUG("failed removing auction %03d directory on rollback action, a ghost auction now exists", auc_count);
    }
 
    auc_count--;
    return;
}


/**
* This is a very big operation, it could be more granular.
*/
int create_new_auction(char *uid, char *name, char *fname, int sv, int ta, int fsize, int fd) {
    int auc_id;
    if ((auc_id = create_auction_dir()) < 0) {
        rollback_auction_dir_creation();
        return -1;
    }

    // create START_AID.txt file
    char start_file_path[256];
    sprintf(start_file_path, "AUCTIONS/%03d/START_%03d.txt", auc_count, auc_count);
    if (open(start_file_path, O_CREAT | O_WRONLY, SERVER_MODE) < 0) {
        LOG_ERROR("open: %s", strerror(errno));
        rollback_auction_dir_creation();
        return -1;
    }

    // process date
    time_t epoch_time; 
    if ((epoch_time = time(NULL)) == ((time_t ) - 1)) {
        LOG_ERROR("time: %s", strerror(errno));
        rollback_auction_dir_creation();
        return -1;
    }

    struct tm *tm_time;
    if ((tm_time = gmtime(&epoch_time)) == NULL) {
        rollback_auction_dir_creation();
        return -1;
    }

    char str_time[20];
    sprintf(str_time, "%4d-%02d-%02d %02d:%02d:%02d", 
                        tm_time->tm_year + 1900, tm_time->tm_mon + 1, tm_time->tm_mday,
                        tm_time->tm_hour, tm_time->tm_min, tm_time->tm_sec);


    char start_data[256];
    sprintf(start_data, "%s %s %s %d %d %s %ld\n", 
                            uid, name, fname, sv, ta, str_time, epoch_time);

    // write data to START_AID.txt file
    FILE *fp;
    if ((fp = fopen(start_file_path, "w")) == NULL) {
        rollback_auction_dir_creation();
        return -1;
    }

    if (fputs(start_data, fp) < 0) {
        fclose(fp);
        rollback_auction_dir_creation();
        return -1;
    }

    fclose(fp);

    return 0;
}


int main() {
    // init_database();
    // int r = create_new_auction("gonga", "name", "fname", 1234, 1234);
    // if (r != 0) {
    //     printf("failed");
    // }
    // r = create_new_auction("gonga", "name", "fname", 1234, 1234);
    // if (r != 0) {
    //     printf("failed");
    // }
}