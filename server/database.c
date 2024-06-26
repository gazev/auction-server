#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <dirent.h>
#include <time.h>
#include <sys/socket.h>

#include <pthread.h>
#include <sys/stat.h>

#include "../utils/constants.h"
#include "../utils/config.h"
#include "../utils/logging.h"
#include "../utils/validators.h"
#include "../utils/utils.h"

#include "database.h"


static const mode_t SERVER_MODE = S_IREAD | S_IWRITE | S_IEXEC;

static int auc_count = 0;
int load_db_state();

static pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;
int lock_db_mutex(char *resource);
int unlock_db_mutex(char *resource);

/**
* Initializes DB. Returns 0 on success and -1 on fatal error.
*/
int init_database() {
    if (mkdir(DB_ROOT, SERVER_MODE) != 0) {
        if (errno != EEXIST) {
            LOG_ERROR("[DB] Failed creating database directory");
            LOG_ERROR("[DB] mkdir: %s", strerror(errno));
            return -1;;
        }
    }

    if (chdir(DB_ROOT) != 0) {
        LOG_ERROR("[DB] chdir: %s", strerror(errno));
        return -1;
    }

    // create USERS dir
    if (mkdir("USERS", SERVER_MODE) != 0) {
        if (errno != EEXIST) {
            LOG_ERROR("[DB] Failed creating USERS directory in database");
            LOG_ERROR("[DB] mkdir: %s", strerror(errno));
            return -1;
        }
    }

    // create AUCTIONS dir
    if (mkdir("AUCTIONS", SERVER_MODE) != 0) {
        if (errno != EEXIST) {
            LOG_ERROR("[DB] Failed creating AUCTIONS directory");
            LOG_ERROR("[DB] mkdir: %s", strerror(errno));
            return -1;
        }
    }

    // initialize DB state
    if (load_db_state() != 0) {
        LOG_ERROR("[DB] Failed setting databse state");
        return -1;
    };

    return 0;
}

int load_db_state() {
    DIR *dp;
    struct dirent *cur;

    if ((dp = opendir("AUCTIONS")) == NULL) {
        LOG_ERROR("[DB] Failed loading database state");
        LOG_ERROR("[DB] open: %s", strerror(errno));
        return -1;
    }

    // count all auctions in DB
    while ((cur = readdir(dp)) != NULL) {
        if (cur->d_name[0] != '.') auc_count++;
    }

    if (closedir(dp) != 0) {
        LOG_DEBUG("[DB] Failed closing DIR *dp, resources may be leaking");
        LOG_DEBUG("[DB] closedir: %s", strerror(errno));
    };

    return 0;
}

/**
* Updates the databse by closing auctions that are expired.
*/
int update_database() {
    LOG_DEBUG("[DB] Updating database");
    lock_db_mutex("update");

    struct dirent **entries;
    int n_entries = scandir("AUCTIONS", &entries, NULL, NULL);
    if (n_entries < 0) {
        LOG_DEBUG("[DB] Failed retrieving user auctions");
        LOG_DEBUG("[DB] scandir: %s", strerror(errno));
        unlock_db_mutex("list");
        return -1;
    }

    /** 
    * Iteratively check if each auction has already ended
    */
    for (int i = 0; i < n_entries; i++) {
        if (entries[i]->d_name[0] == '.') {
            free(entries[i]);
            continue;
        }

        char bid_path[32];
        sprintf(bid_path, "AUCTIONS/%.3s/END_%.3s.txt", entries[i]->d_name, entries[i]->d_name);
        int end_fd;

        // bid has already ended, continue
        if ((end_fd = open(bid_path, O_RDONLY)) > 0) {
            if (close(end_fd) != 0) {
                LOG_DEBUG("[DB] Failed closing file descriptor, resources might be leaking");
                LOG_DEBUG("[DB] close: %s", strerror(errno));
            }
            free(entries[i]);
            continue;
        }

        /**
        * Get information from START file
        */
        FILE *fp;
        sprintf(bid_path, "AUCTIONS/%.3s/START_%.3s.txt", entries[i]->d_name, entries[i]->d_name);
        if ((fp = fopen(bid_path, "r")) == NULL) {
            LOG_DEBUG("[DB] Failed opening bid information file");
            LOG_DEBUG("[DB] fopen: %s", strerror(errno));
            free(entries[i]);
            continue;
        }

        char bid_info[128];
        fgets(bid_info, 128, fp);
        fclose(fp);

        // read auction start time and end time 
        char *time_active;
        char *start_time;

        // time active is the 5th word in the file
        strtok(bid_info, " ");    
        for (int i = 0; i < 4; i++) {
            time_active = strtok(NULL, " ");    
        }

        // start time is the 8th
        for (int i = 0; i < 3; ++i) {
            start_time = strtok(NULL, " ");
        }

        // valid both
        if (time_active == NULL || start_time == NULL) {
            LOG_DEBUG("Got invaild time active and start time from bid file %s", entries[i]->d_name);
            free(entries[i]);
            continue;
        }

        if (!is_valid_time_active(time_active)) {
            LOG_DEBUG("Got invaild time active from bid file %s", entries[i]->d_name);
            free(entries[i]);
            continue;
        }

        int time_active_i = atoi(time_active);
        int start_time_l = atol(start_time);
        long curr_time = time(NULL);

        if (start_time_l == 0) {
            LOG_DEBUG("Got invaild start time from bid file %s", entries[i]->d_name);
        }

        /** 
        * Check for auction expiration, if not expired, continue
        */
        if (curr_time - start_time_l < time_active_i) {
            free(entries[i]);
            continue;
        }

        /** 
        * If auction expired, create END file and write to it the date time of 
        * the auction end and the time in seconds it remained active
        */
        // create END file
        int close_fd;
        sprintf(bid_path, "AUCTIONS/%.3s/END_%.3s.txt", entries[i]->d_name, entries[i]->d_name);
        if ((close_fd = open(bid_path, O_CREAT | O_WRONLY, SERVER_MODE)) < 0) {
            LOG_DEBUG("[DB] Failed creating END_%.3s.txt file", entries[i]->d_name);
            LOG_DEBUG("[DB] open: %s", strerror(errno));
            free(entries[i]);
            continue;
        }

        if (close(close_fd) != 0) {
            LOG_DEBUG("[DB] Failed closing file descriptor, resources might be leaking");
            LOG_DEBUG("[DB] close: %s", strerror(errno));
        }

        // get datetime to put in END file
        long end_sec_time = start_time_l + time_active_i;
        struct tm *tm_time;
        if ((tm_time = gmtime(&end_sec_time)) == NULL) {
            LOG_DEBUG("[DB] Couldn't get current time information")
            free(entries[i]);
            continue;
        }

        char time_str[128];
        sprintf(time_str, "%4d-%02d-%02d %02d:%02d:%02d", 
                    tm_time->tm_year + 1900, tm_time->tm_mon + 1, tm_time->tm_mday,
                    tm_time->tm_hour, tm_time->tm_min, tm_time->tm_sec);

        char end_info[256];
        sprintf(end_info, "%s %d\n", time_str, time_active_i);
 
        // write information to END file
        if ((fp = fopen(bid_path, "w")) == NULL) {
            LOG_DEBUG("[DB] Failed opening bid end information file");
            LOG_DEBUG("[DB] fopen: %s", strerror(errno));
            free(entries[i]);
            continue;
        }

        fputs(end_info, fp);
        fclose(fp);
        free(entries[i]);
    }

    free(entries);

    unlock_db_mutex("list");
    return 0;
}

/**
* Check if user is registred in DB 
*/
int exists_user(char *uid) {
    char user_passwd_path[256];
    FILE *fp;

    lock_db_mutex(uid);
    // check password file
    sprintf(user_passwd_path, "USERS/%6s/%6s_pass.txt", uid, uid);
    if ((fp = fopen(user_passwd_path, "r")) == NULL) {
        unlock_db_mutex(uid);
        return 0;
    }

    fclose(fp);

    unlock_db_mutex(uid);

    return 1;
}

/**
* Checks if user is logged in
*/
int is_user_logged_in(char *uid) {
    lock_db_mutex(uid);

    FILE *fp;
    char user_login_path[64];
    sprintf(user_login_path, "USERS/%6s/%6s_login.txt", uid, uid);
    if ((fp = fopen(user_login_path, "r")) == NULL) {
        unlock_db_mutex(uid);
        return 0;
    }

    fclose(fp);

    unlock_db_mutex(uid);
    return 1;
}

/**
* Check if an auction exists in the database (this is like this for now, must check later)
*/
int exists_auction(char *aid) {
    lock_db_mutex(aid);

    int aid_int = atoi(aid);
    int ret = (aid_int <= auc_count && aid_int > 0);

    unlock_db_mutex(aid);
    return ret; 
}

int is_auction_finished(char *aid) {
    lock_db_mutex(aid);

    FILE *fp;
    char auc_path[64];
    sprintf(auc_path, "AUCTIONS/%3s/END_%3s.txt", aid, aid);
    if ((fp = fopen(auc_path, "r")) == NULL) {
        unlock_db_mutex(aid);
        return 0;
    }

    fclose(fp);

    unlock_db_mutex(aid);
    return 1;
}

int get_auction_info(char *aid, char *buff, int n) {
    lock_db_mutex(aid);

    FILE *fp;
    char auction_path[64];
    sprintf(auction_path, "AUCTIONS/%3s/START_%3s.txt", aid, aid);
    if ((fp = fopen(auction_path, "r")) == NULL) {
        LOG_DEBUG("[DB] Failed reading from auction %s information file, databsae might be corrupted", aid);
        unlock_db_mutex(aid);
        return 1;
    }

    if (fgets(buff, n, fp) == NULL) {
        fclose(fp);
        LOG_DEBUG("[DB] Failed reading from auction %s information file, database might be corrupted", aid);
        unlock_db_mutex(aid);
        return 1;
    };

    fclose(fp);

    unlock_db_mutex(aid);

    return 0;
}

int get_user_auctions(char *uid, char *buff) {
    lock_db_mutex(uid);

    struct dirent **entries;
    char user_hosted_path[32];
    sprintf(user_hosted_path, "USERS/%.6s/HOSTED", uid);
    int n_entries = scandir(user_hosted_path, &entries, NULL, alphasort);
    if (n_entries < 0) {
        LOG_DEBUG("[DB] Failed retrieving user auctions");
        LOG_DEBUG("[DB] scandir: %s", strerror(errno));
        unlock_db_mutex(uid);
        return -1;
    }

    int written = 0;
    char hosted_auc_path[32];
    int check_fd;
    for (int i = 0; i < n_entries; i++) {
        // ignore hidden files
        if (entries[i]->d_name[0] == '.') continue;

        char asset_status[128];
        // check each auction status (if it's ended)
        sprintf(hosted_auc_path, "AUCTIONS/%.3s/END_%.3s.txt", entries[i]->d_name, entries[i]->d_name);
        if ((check_fd = open(hosted_auc_path, 0, SERVER_MODE)) < 0) {
            sprintf(asset_status, " %.3s 1", entries[i]->d_name); // not ended
        } else {
            sprintf(asset_status, " %.3s 0", entries[i]->d_name); // ended
            if (close(check_fd) != 0) {
                LOG_DEBUG("[DB] Failed closing file descriptor, resources might be leaking");
                LOG_DEBUG("[DB] close: %s", strerror(errno));
            };
        }
        strcat(buff, asset_status);
        written += strlen(asset_status);

        free(entries[i]);
    }
    
    free(entries);

    strcat(buff, "\n");
    written += 1;

    unlock_db_mutex(uid);

    return written;
}

int get_auctions_list(char *buff) {
    lock_db_mutex("list");

    struct dirent **entries;
    int n_entries = scandir("AUCTIONS", &entries, NULL, alphasort);
    if (n_entries < 0) {
        LOG_DEBUG("[DB] Failed retrieving user auctions");
        LOG_DEBUG("[DB] scandir: %s", strerror(errno));
        unlock_db_mutex("list");
        return -1;
    }

    /** Iterate all auctions */
    int written = 0;
    char curr_auc_path[32];
    int check_fd;
    for (int i = 0; i < n_entries; i++) {
        if (entries[i]->d_name[0] == '.') continue;

        // path to current auction "AUCTIONS/001/END_001.txt"
        sprintf(curr_auc_path, "AUCTIONS/%.3s/END_%.3s.txt", entries[i]->d_name, entries[i]->d_name);
        char auc_status[8];
        if ((check_fd = open(curr_auc_path, 0, SERVER_MODE)) < 0) {
            sprintf(auc_status, " %.3s 1", entries[i]->d_name); // not ended
        } else {
            sprintf(auc_status, " %.3s 0", entries[i]->d_name); // ended
            if (close(check_fd) != 0) {
                LOG_DEBUG("[DB] Failed closing file descriptor, resources might be leaking")
                LOG_DEBUG("[DB] closedir: %s", strerror(errno));
            };
        }

        strcat(buff, auc_status);
        written += strlen(auc_status);

        free(entries[i]);
    }

    free(entries);

    strcat(buff, "\n");
    written += 1;

    unlock_db_mutex("list");


    return written;
}

int get_auction_bidders_list(char *aid, char *buff) {
    lock_db_mutex("bidders list");

    struct dirent **entries;
    char bids_path[32];
    sprintf(bids_path, "AUCTIONS/%.3s/BIDS", aid);
    int n_entries = scandir(bids_path, &entries, NULL, alphasort);
    if (n_entries < 0) { // couldn't scan directory
        LOG_DEBUG("Failed retrieving auction %s bids", aid);
        LOG_DEBUG("scandir: %s", strerror(errno));
        unlock_db_mutex("bidders list");
        return -1;
    }

    // get rid of '.' and '..'
    free(entries[0]);
    free(entries[1]);

    // we start after '.' and '..', we assume alphabetically they are always first, if no one
    // tampers with database manually this is true
    int first_entry = 2;
    // if more than 50 auctions exist we retrieve the most 50 recent ones, 50 biggest bid values
    if (n_entries > 52)
        first_entry = n_entries - 50;

    int written = 0;         // bytes written to buff
    char curr_bid_path[32]; // bid path 
    char bid_info[128];      // information about currently iterated bid
    for (int i = first_entry; i < n_entries; ++i) {
        FILE *fp;
        // iterated bid "AUCTIONS/001/BIDS/000100.txt"
        sprintf(curr_bid_path, "AUCTIONS/%.3s/BIDS/%.10s", aid, entries[i]->d_name);
        if ((fp = fopen(curr_bid_path, "r")) == NULL) {
            LOG_DEBUG("Failed opening bid file");
            free(entries[i]);
            continue;
        }
        
        // retrieve information about bid
        if (fgets(bid_info, 128, fp) < 0) {
            LOG_DEBUG("Failed getting bid information");
            fclose(fp);
            free(entries[i]);
            continue;
        };

        fclose(fp);

        // first field, bidder UID
        char *bidder_uid = strtok(bid_info, " ");
        if (bidder_uid == NULL) {
            LOG_DEBUG("Got null bidder uid");
            free(entries[i]);
            continue;
        }

        if (!is_valid_uid(bidder_uid)) {
            LOG_DEBUG("Got invalid bidder UID")
            free(entries[i]);
            continue;
        }

        // second field, bid date in format YYYY-MM-DD
        char *bid_date = strtok(NULL, " ");
        if (bid_date == NULL) {
            LOG_DEBUG("Got no bid date");
            free(entries[i]);
            continue;
        }

        // third field, bid time in format HH:MM:SS
        char *bid_time = strtok(NULL, " ");
        if (bid_time == NULL) {
            LOG_DEBUG("Got no bid time");
            free(entries[i]);
            continue;
        }

        if (!is_valid_date_time(bid_date, bid_time)) {
            LOG_DEBUG("Invalid date time");
            free(entries[i]);
            continue;
        }

        // last field, seconds passed from auction start until bid was made
        char *bid_sec_time = strtok(NULL, "\n");
        if (bid_sec_time == NULL) {
            LOG_DEBUG("Got not bid sec time");
            free(entries[i]);
            continue;
        }

        // retrieve bid value from file name, file name in format VVVVVV.txt
        int bid_value;
        if ((sscanf(entries[i]->d_name, "%d.txt", &bid_value)) != 1) {
            LOG_DEBUG("Invalid bid value");
            free(entries[i]);
            continue;
        }

        char bid_entry[128] = {0};
        // write entry to response { B host_UID bid_value bid_date-time bid_sec_time }
        sprintf(bid_entry, " B %.6s %d %s %s %s", bidder_uid, bid_value, bid_date, bid_time, bid_sec_time);
        strcat(buff, bid_entry);
        written += strlen(bid_entry);

        free(entries[i]);
    }

    // free non interated bids
    for (int i = 2; i < first_entry; ++i)
        free(entries[i]);

    free(entries);
    
    /**
    * Check if auction has ended
    */
    FILE *fp;
    char end_path[32];
    char end_info[128] = {0};
    sprintf(end_path, "AUCTIONS/%.3s/END_%.3s.txt", aid, aid);
    if ((fp = fopen(end_path, "r")) != NULL) {
        // retrieve bid END information
        char end_buff[128];
        if (fgets(end_buff, 128, fp) != NULL) {
            char *date = strtok(end_buff, " ");
            char *time = strtok(NULL, " ");
            char *end_sec_time = strtok(NULL, "\n");

            // this was the last part of the code written for the entire thing, the fatigue is palpable
            if (date != NULL && time != NULL && end_sec_time != NULL) {
                if (is_valid_date_time(date, time)) {
                    sprintf(end_info, " E %s %s %s", date, time, end_sec_time);
                    written += strlen(end_info);

                }
            }
        }
        fclose(fp);
    }
    
    strcat(buff, end_info);
    strcat(buff, "\n");
    written += 1;

    unlock_db_mutex("bidders list");
    return written;
}

/**
* Logs in the user with uid. Returns 0 on success and -1 on failure
*/
int log_in_user(char *uid) {
    lock_db_mutex(uid);

    int fd;
    char user_login_path[32];
    sprintf(user_login_path, "USERS/%6s/%6s_login.txt", uid, uid);
    if ((fd = open(user_login_path, O_CREAT, SERVER_MODE)) == -1) {
        LOG_DEBUG("[DB] Couldn't create login file for user %s", uid);
        unlock_db_mutex(uid);
        return -1;
    }

    if (close(fd) != 0) {
        LOG_DEBUG("[DB] Failed closing file descriptor, resources might be leaking");
        LOG_DEBUG("[DB] close: %s", strerror(errno));
    };

    unlock_db_mutex(uid);
    return 0;
}

int log_out_user(char *uid) {
    lock_db_mutex(uid);

    char user_login_path[32];
    sprintf(user_login_path, "USERS/%.6s/%.6s_login.txt", uid, uid);
    if (remove(user_login_path) != 0) {
        LOG_DEBUG("[DB] Failed removing login file %s", uid);
        LOG_DEBUG("[DB] remove: %s", strerror(errno));
        unlock_db_mutex(uid);
        return -1;
    }

    unlock_db_mutex(uid);
    return 0;
}

/**
* Creates a user in the DB. Returns 0 on success and -1 on failure
*/
int register_user(char *uid, char *passwd) {
    lock_db_mutex(uid);
    
    // create user directory (e.g root/USERS/123456)
    char user_file_path[32];
    sprintf(user_file_path, "USERS/%.6s", uid);
    if (mkdir(user_file_path, SERVER_MODE) != 0) {
        if (errno != EEXIST) {
            LOG_DEBUG("[DB] Couldn't create user directory for user %s", uid);
            LOG_DEBUG("[DB] mkdir: %s", strerror(errno));
            unlock_db_mutex(uid);
            return -1;
        }
    }
    
    // create user's HOSTED dir
    sprintf(user_file_path, "USERS/%.6s/HOSTED", uid);
    if (mkdir(user_file_path, SERVER_MODE) != 0) {
        if (errno != EEXIST) {
            LOG_DEBUG("[DB] Couldn't create HOSTED directory for user %s", uid);
            LOG_DEBUG("[DB] mkdir: %s", strerror(errno));
            unlock_db_mutex(uid);
            return -1;
        }
    }
    
    // create user's BIDDED dir
    sprintf(user_file_path, "USERS/%6s/BIDDED", uid);
    if (mkdir(user_file_path, SERVER_MODE) != 0) {
        if (errno != EEXIST) {
            LOG_DEBUG("[DB] Couldn't create BIDDED directory for user %s", uid);
            LOG_DEBUG("[DB] mkdir: %s", strerror(errno));
            unlock_db_mutex(uid);
            return -1;
        }
    }

       // mark user as logged in (e.g touch root/USERS/123456/123456_login.txt)
    int login_fd;
    sprintf(user_file_path, "USERS/%6s/%6s_login.txt", uid, uid);
    if ((login_fd = open(user_file_path, O_CREAT, SERVER_MODE)) < 0) {
        LOG_DEBUG("[DB] Couldn't create login file for user %6s", uid);
        LOG_DEBUG("[DB] open: %s", strerror(errno));
        unlock_db_mutex(uid);
        return -1;
    }

    if (close(login_fd) != 0) {
        LOG_DEBUG("[DB] Failed closing file descriptor, resources might be leaking");
        LOG_DEBUG("[DB] close: %s", strerror(errno));
    };

    // create user password file (e.g root/USERS/123456/123456_pass.txt)
    sprintf(user_file_path, "USERS/%6s/%6s_pass.txt", uid, uid);
    int pass_fd;
    if ((pass_fd = open(user_file_path, O_CREAT | O_WRONLY, SERVER_MODE)) < 0) {
        LOG_ERROR("[DB] open: %s", strerror(errno));
        LOG_DEBUG("[DB] Couldn't create user password file for user %s", uid);
        unlock_db_mutex(uid);
        return -1;
    }

    // write password
    if (write(pass_fd, passwd, 8) != 8) {
        LOG_DEBUG("[DB] Couldn't write password for user %s", uid);
        LOG_ERROR("[DB] write: %s", strerror(errno));
        if (remove(user_file_path) != 0) {
            LOG_DEBUG("[DB] Failed removing %s_pass.txt after failure registering user, a ghost user %s now exists", uid, uid);
        }

        if (close(pass_fd) != 0) {
            LOG_DEBUG("[DB] Failed closing file descriptor, resources might be leaking");
            LOG_DEBUG("[DB] close: %s", strerror(errno));
        }

        unlock_db_mutex(uid);
        return -1;
    }
 
    if (close(pass_fd) != 0) {
        LOG_DEBUG("[DB] Failed closing file descriptor, resources might be leaking");
        LOG_DEBUG("[DB] close: %s", strerror(errno));
    };

    unlock_db_mutex(uid);
    return 0;
}

/**
* Unregister a user. Returns 0 on success and -1 on failure, which may screw the
* user's bids and hosted auctions. It would take way too much work to use tmp files for this, also,
* databases are so complex to implement we simply assume it's not necessary for 
* this project
*/
int unregister_user(char *uid) {
    lock_db_mutex(uid);

    // remove user's login
    char user_file_path[32];
    sprintf(user_file_path, "USERS/%6s/%6s_login.txt", uid, uid);
    if (remove(user_file_path) != 0) {
        LOG_DEBUG("[DB] Failed removing login file for user %s", uid);
        LOG_DEBUG("[DB] remove: %s", strerror(errno));
        unlock_db_mutex(uid);
        return -1;
    }

    // remove user's passwd
    sprintf(user_file_path, "USERS/%.6s/%.6s_pass.txt", uid, uid);
    if (remove(user_file_path) != 0) {
        LOG_DEBUG("[DB] Failed removing password file for user %s", uid);
        LOG_DEBUG("[DB] remove: %s", uid);
        unlock_db_mutex(uid);
        return -1;
    }

    unlock_db_mutex(uid);
    return 0;
}

/**
* Returns 1 if provided password matches the password stored for user with uid
*/
int is_authentic_user(char *uid, char *passwd) {
    lock_db_mutex(uid);

    FILE *fp;
    char passwd_path_file[32];
    char stored_password[16];
    sprintf(passwd_path_file, "USERS/%.6s/%.6s_pass.txt", uid, uid);
    if ((fp = fopen(passwd_path_file, "r")) == NULL) {
        unlock_db_mutex(uid);
        return 0;
    }

    if (fgets(stored_password, 16, fp) == NULL) {
        fclose(fp);
        unlock_db_mutex(uid);
        return -1;
    }

    fclose(fp);

    int r = strcmp(stored_password, passwd) == 0 ? 1 : 0;

    unlock_db_mutex(uid);
    return r;
}

/**
* Creates a new auction and returns it's AID if successfull. On failure returns -1
*/
int create_auction_dir() { 
    // if we reached the limit auctions
    if (auc_count >= 999) {
        return -1;
    }

    auc_count++;

    char tmp_path[32];
    // create auction directory    
    sprintf(tmp_path, "AUCTIONS/%03d", auc_count);
    if (mkdir(tmp_path, SERVER_MODE) != 0) {
        if (errno != EEXIST) {
            LOG_DEBUG("[DB] Failed creating new auction %s", tmp_path);
            LOG_DEBUG("[DB] mkdir: %s", strerror(errno));
            return -1;
        }
    }

    // create BIDS folder inside dir
    sprintf(tmp_path, "AUCTIONS/%03d/BIDS", auc_count);
    if (mkdir(tmp_path, SERVER_MODE) != 0) {
        if (errno != EEXIST) {
            LOG_DEBUG("[DB] Failed creating BIDS folder for auction %s", tmp_path);
            LOG_DEBUG("[DB] mkdir: %s", strerror(errno));
            return -1;
        }
    }

    // create BIDS folder inside dir
    sprintf(tmp_path, "AUCTIONS/%03d/ASSET", auc_count);
    if (mkdir(tmp_path, SERVER_MODE) != 0) {
        if (errno != EEXIST) {
            LOG_DEBUG("[DB] Failed creating ASSET folder for auction %s", tmp_path);
            LOG_DEBUG("[DB] mkdir: %s", strerror(errno));
            return -1;
        }
    }

    return auc_count;
}

/**
* Rolls back an auction directory creation that went wrong. This prevents 
* ghost auctions from being accumulated in the database
*/
void rollback_auction_dir_creation() {
    DIR *dp;
    struct dirent *cur;

    // directory for auction doesn't exist (creation failed because max limit was exceeded)
    char auction_file_path[32];
    sprintf(auction_file_path, "AUCTIONS/%03d", auc_count);
    if ((dp = opendir(auction_file_path)) == NULL) {
        if (errno == ENOENT) { // directory doesn't exist
            LOG_DEBUG("[DB] Dir doesn't exist / wasn't created (%s)", auction_file_path);
        } else {
            LOG_DEBUG("[DB] Failed opening %03d auction directory on rollback action, database might be corrupted", auc_count);
            LOG_DEBUG("[DB] open: %s", strerror(errno));
        }
        return;
    }

    char file_path[512]; // it's this big because any file can actually be placed there (even if statement prohibits this fname size)
                        // and we must avoid buffer overflows
    // remove all regular files inside directory
    while ((cur = readdir(dp)) != NULL) {
        if (cur->d_name[0] == '.') continue;

        // remove all files
        if (cur->d_type == DT_REG) {
            sprintf(file_path, "AUCTIONS/%03d/%s", auc_count, cur->d_name);
            if (remove(file_path) != 0) {
                LOG_DEBUG("[DB] Couldn't remove file %s on rollback action, database might be corrupted", cur->d_name);
                LOG_DEBUG("[DB] remove: %s", strerror(errno));
            }
        }
    }

    if (closedir(dp) != 0) {
        LOG_DEBUG("[DB] Failed closing file descriptor, resources may be leaking");
        LOG_DEBUG("[DB] closedir: %s", strerror(errno));
    };

    // remove bids if they exist
    char bids_dir[32];
    char bid_file_path[512]; // this because becuase of same reason as `file_path`
    sprintf(bids_dir, "AUCTIONS/%03d/BIDS", auc_count);
    if ((dp = opendir(bids_dir)) != NULL) {
        while ((cur = readdir(dp)) != NULL) {
            if (cur->d_name[0] == '.') continue;

            if (cur->d_type == DT_REG) {
                sprintf(bid_file_path, "AUCTIONS/%03d/BIDS/%s", auc_count, cur->d_name);
                if (remove(bid_file_path) != 0) {
                    LOG_DEBUG("[DB] Couldn't remove bid file %s on rollback action, database might be corrupted", cur->d_name);
                    LOG_ERROR("[DB] remove: %s", strerror(errno));
                }
            }

        }

        if (closedir(dp) != 0) {
            LOG_DEBUG("[DB] Failed closing file descriptor, resources may be leaking");
            LOG_DEBUG("[DB] closedir: %s", strerror(errno));
        };

        // remove directory
        if ((remove(bids_dir) != 0)) {
            LOG_DEBUG("[DB] Failed removing BIDS directory on rollback action, a ghost auction now exists");
            LOG_DEBUG("[DB] remove: %s", strerror(errno));
        }
    }

    // remove files in ASSET folder
    char asset_dir[32];
    char asset_file_path[512]; // this big because of the same reason as `file_path`
    sprintf(asset_dir, "AUCTIONS/%03d/ASSET", auc_count);
    if ((dp = opendir(asset_dir)) != NULL) {
        while ((cur = readdir(dp)) != NULL) {
            if (cur->d_name[0] == '.') continue;

            if (cur->d_type == DT_REG) {
                sprintf(asset_file_path, "AUCTIONS/%03d/ASSET/%s", auc_count, cur->d_name);
                if (remove(asset_file_path) != 0) {
                    LOG_DEBUG("[DB] Couldn't remove bid file %s on rollback action, database might be corrupted", cur->d_name);
                    LOG_DEBUG("[DB] remove: %s", strerror(errno));
                }
            }
        }

        if (closedir(dp) != 0) {
            LOG_DEBUG("[DB] Failed closing file descriptor, resources may be leaking");
            LOG_DEBUG("[DB] closedir: %s", strerror(errno));
        }

        if ((remove(asset_dir) != 0)) {
            LOG_DEBUG("[DB] Failed removing ASSET directory on rollback action, a ghost auction now exists");
            LOG_DEBUG("[DB] remove: %s", strerror(errno));
        }
    }

    // remove directory
    if ((remove(auction_file_path) != 0)) {
        LOG_DEBUG("[DB] Failed removing directory on rollback action, a ghost auction now exists");
        LOG_DEBUG("[DB] remove: %s", strerror(errno));
    }
 
    auc_count--;

    return;
}


/**
* Opens an auction in the database. 
* This funciton handles the logic of creating an auction in the database and also
* downloads the asset into the auction.
*/
int create_new_auction(char *uid, char *name, char *fname, int sv, int ta, int fsize, int conn_fd) {
    lock_db_mutex("create_auction");

    int auc_id;
    if ((auc_id = create_auction_dir()) < 0) {
        unlock_db_mutex("create_auction");
        return -1;
    }

    // create START_AID.txt file
    char tmp_path[64];
    sprintf(tmp_path, "AUCTIONS/%03d/START_%03d.txt", auc_count, auc_count);
    int sfd;
    if ((sfd = open(tmp_path, O_CREAT | O_WRONLY, SERVER_MODE)) < 0) {
        LOG_DEBUG("open: %s", strerror(errno));
        rollback_auction_dir_creation();
        unlock_db_mutex("create_auction");
        return -1;
    }

    if (close(sfd) != 0) {
        LOG_DEBUG("[DB] Failed closing file descriptor, resources may be leaking");
        LOG_DEBUG("close: %s", strerror(errno));
    };

    /**
    * Process auction information
    */
    // auction start time
    time_t unix_start_time;
    if ((unix_start_time = time(NULL)) == ((time_t ) - 1)) {
        LOG_DEBUG("[DB] Failed getting UNIX timestamp");
        LOG_DEBUG("[DB] time: %s", strerror(errno));
        rollback_auction_dir_creation();
        unlock_db_mutex("create_auction");
        return -1;
    }

    // auction start datetime
    struct tm *tm_time;
    if ((tm_time = gmtime(&unix_start_time)) == NULL) {
        LOG_DEBUG("[DB] Failed getting datetime");
        rollback_auction_dir_creation();
        unlock_db_mutex("create_auction");
        return -1;
    }

    char str_time[128];
    sprintf(str_time, "%4d-%02d-%02d %02d:%02d:%02d", 
                        tm_time->tm_year + 1900, tm_time->tm_mon + 1, tm_time->tm_mday,
                        tm_time->tm_hour, tm_time->tm_min, tm_time->tm_sec);


    char start_info[256];
    sprintf(start_info, "%s %s %s %d %d %s %ld\n", 
                            uid, name, fname, sv, ta, str_time, unix_start_time);

    /**
    * Write auction information
    */
    FILE *fp;
    if ((fp = fopen(tmp_path, "w")) == NULL) {
        rollback_auction_dir_creation();
        unlock_db_mutex("create_auction");
        return -1;
    }

    if (fputs(start_info, fp) < 0) {
        fclose(fp);
        rollback_auction_dir_creation();
        unlock_db_mutex("create_auction");
        return -1;
    }

    fclose(fp);

    /**
    * Retrieve auction asset file
    */
    char asset_fname_path[64];
    sprintf(asset_fname_path, "AUCTIONS/%03d/ASSET/%.*s", auc_count, FNAME_LEN, fname);
    int afd;
    if ((afd = open(asset_fname_path, O_CREAT | O_WRONLY, SERVER_MODE)) < 0) {
        LOG_DEBUG("[DB] open: %s", strerror(errno));
        rollback_auction_dir_creation();
        unlock_db_mutex("create_auction");
        return -1;
    }

    /**
    * Read asset content from socket
    */
    if (as_recv_asset_file(afd, conn_fd, fsize) != 0) {
        LOG_DEBUG("[DB] Failed receiving assetfile when creating new auction ")
        close(afd);
        rollback_auction_dir_creation();
        unlock_db_mutex("create_auction");
        return -1;
    }

    // Write last block without the \n 
    if (close(afd) != 0) {
        LOG_DEBUG("[DB] Failed closing file descriptor, resources may be leaking");
        LOG_DEBUG("[DB] close: %s", strerror(errno));
    };

    /**
    * Register user auction
    */
    sprintf(tmp_path, "USERS/%6s/HOSTED/%03d.txt", uid, auc_id);
    int tmp_fd;
    if ((tmp_fd = open(tmp_path, O_CREAT | O_WRONLY, SERVER_MODE)) < 0) {
        LOG_DEBUG("[DB] open: %s", strerror(errno));
        rollback_auction_dir_creation();
        unlock_db_mutex("create_auction");
        return -1;
    }

    if (close(tmp_fd) != 0) {
        LOG_DEBUG("[DB] Failed closing file descriptor, resources may be leaking");
        LOG_DEBUG("[DB] close: %s", strerror(errno));
    };

    unlock_db_mutex("create_auction");
    return auc_id;
}

int close_auction(char *aid) {
    lock_db_mutex(aid);
    
    FILE *fp;
    char auction_path[128];
    sprintf(auction_path, "AUCTIONS/%3s/START_%3s.txt", aid, aid);
    if ((fp = fopen(auction_path, "r")) == NULL) {
        LOG_DEBUG("[DB] Failed reading from auction %s information file, databsae might be corrupted", aid);
        unlock_db_mutex(aid);
        return 1;
    }

    char auction_info[256];
    if (fgets(auction_info, 128, fp) == NULL) {
        LOG_DEBUG("[DB] Failed reading from auction %s information file, database might be corrupted", aid);
        fclose(fp);
        unlock_db_mutex(aid);
        return 1;
    };

    fclose(fp);

    /**
    * Calculate the time the auction remained active
    */
    // read last entry in START file (the start unix timestamp of the auction)
    char *start_time = strtok(auction_info, " ");
    for (int i = 0; i < 6; ++i) {
        start_time = strtok(NULL, " ");
    }

    start_time = strtok(NULL, "\n");

    if (start_time == NULL) {
        LOG_DEBUG("[DB] Got a badly formatted START_%3s file", aid );
        unlock_db_mutex(aid);
        return -1;
    }

    // convert to long
    long start_time_l = atol(start_time);

    // failed converting (not a numeric type)
    if (start_time_l == 0) {
        LOG_DEBUG("[DB] Got a badly formatted START_%3s file", aid );
        unlock_db_mutex(aid);
        return -1;
    }

    // current time
    time_t curr_time = time(NULL);
    // seconds the auction remained open
    long end_sec_time = (long)curr_time - start_time_l;

    struct tm *tm_time;
    if ((tm_time = gmtime(&curr_time)) == NULL) {
        LOG_DEBUG("[DB] Couldn't get current time information")
        unlock_db_mutex(aid);
        return -1;
    }

    char end_datetime[128];
    sprintf(end_datetime, "%4d-%02d-%02d %02d:%02d:%02d", 
                        tm_time->tm_year + 1900, tm_time->tm_mon + 1, tm_time->tm_mday,
                        tm_time->tm_hour, tm_time->tm_min, tm_time->tm_sec);


    /**
    * Mark auction as ended (create END file)
    */
    int fd;
    char end_file_path[128];
    sprintf(end_file_path, "AUCTIONS/%3s/END_%3s.txt", aid, aid);
    if ((fd = open(end_file_path, O_CREAT | O_WRONLY, SERVER_MODE)) < 0) {
        LOG_DEBUG("[DB] Failed creating auction %s END file ", aid);
        unlock_db_mutex(aid);
        return -1;
    }

    // we do this because we usually use file streams to write to files :l
    if (close(fd) != 0) {
        LOG_DEBUG("[DB] Failed closing auction %s creation file descriptor, resources may be leaking", aid);
        LOG_DEBUG("[DB] open: %s", strerror(errno));
    }

    /**
    * Write auction end information
    */
    char end_data[258];
    sprintf(end_data, "%s %ld", end_datetime, end_sec_time);
    if ((fp = fopen(end_file_path, "w")) == NULL) {
        LOG_DEBUG("[DB] Failed writting information to auction %s END file", aid);
        unlock_db_mutex(aid);
        return -1;
    }

    if (fputs(end_data, fp) < 0) {
        LOG_DEBUG("[DB] Failed writting information to auction %s END file", aid);
        fclose(fp);
        unlock_db_mutex(aid);
        return -1;
    }

    fclose(fp);

    unlock_db_mutex(aid);
    return 0;
}

int get_last_bid(char *aid) {
    lock_db_mutex("get last bid");

    char bid_path[128];
    sprintf(bid_path, "AUCTIONS/%3s/BIDS", aid);

    struct dirent **entries;

    int n_entries = scandir(bid_path, &entries, NULL, alphasort);
    if (n_entries < 0) {
        LOG_DEBUG("[DB] Failed retrieving user auctions");
        LOG_DEBUG("[DB] scandir: %s", strerror(errno));
        unlock_db_mutex("list");
        return -1;
    }
    
    int last_bid;
    if (sscanf(entries[n_entries - 1]->d_name, "%d.txt", &last_bid) != 1)
        last_bid = 0; //no bids yet 

    //free entries
    for (int i=0; i<n_entries; i++)
        free(entries[i]);
    free(entries);

    unlock_db_mutex("get last bid");
    return last_bid;
}

int bid(char *aid, char *uid, int value) {
    lock_db_mutex("bid");

    // get starting time
    FILE *fp;
    char auction_path[128];
    sprintf(auction_path, "AUCTIONS/%3s/START_%3s.txt", aid, aid);
    if ((fp = fopen(auction_path, "r")) == NULL) {
        LOG_DEBUG("[DB] Failed reading from auction %s information file, databsae might be corrupted", aid);
        unlock_db_mutex(aid);
        return 1;
    }

    char auction_info[256];
    if (fgets(auction_info, 128, fp) == NULL) {
        LOG_DEBUG("[DB] Failed reading from auction %s information file, database might be corrupted", aid);
        fclose(fp);
        unlock_db_mutex(aid);
        return 1;
    };

    fclose(fp);

    // read last entry in START file (the start unix timestamp of the auction)
    char *start_time = strtok(auction_info, " ");
    for (int i = 0; i < 6; ++i) {
        start_time = strtok(NULL, " ");
    }

    start_time = strtok(NULL, "\n");
    if (start_time == NULL) {
        LOG_DEBUG("[DB] Got a badly formatted START_%3s file", aid );
        unlock_db_mutex(aid);
        return -1;
    }

    // convert to long
    long start_time_l = atol(start_time);

    // failed converting (not a numeric type)
    if (start_time_l == 0) {
        LOG_DEBUG("[DB] Got a badly formatted START_%3s file", aid );
        unlock_db_mutex(aid);
        return -1;
    }

    // current time
    time_t curr_time = time(NULL);
    // seconds elapsed since the beginning of the auction
    long bid_sec_time = (long)curr_time - start_time_l;

    struct tm *tm_time;
    if ((tm_time = gmtime(&curr_time)) == NULL) {
        LOG_DEBUG("[DB] Couldn't get current time information")
        unlock_db_mutex(aid);
        return -1;
    }

    char bid_datetime[128];
    sprintf(bid_datetime, "%4d-%02d-%02d %02d:%02d:%02d", 
                        tm_time->tm_year + 1900, tm_time->tm_mon + 1, tm_time->tm_mday,
                        tm_time->tm_hour, tm_time->tm_min, tm_time->tm_sec);


    int fd;
    char bid_path[128];
    sprintf(bid_path, "AUCTIONS/%3s/BIDS/%06d.txt", aid, value);
    if ((fd = open(bid_path, O_CREAT | O_WRONLY, SERVER_MODE)) < 0) {
        LOG_DEBUG("[DB] Failed creating bid file %s %d", aid, value);
        LOG_DEBUG("close: %s", strerror(errno));
        unlock_db_mutex("bid");
        return -1;
    }

    if (close(fd) != 0) {
        LOG_DEBUG("[DB] Failed closing bid file %s, resources may be leaking", aid);
        LOG_DEBUG("close: %s", strerror(errno));
    }

    char bid_info[256];
    sprintf(bid_info, "%.6s %s %ld\n", uid, bid_datetime, bid_sec_time);

    if ((fp = fopen(bid_path, "w")) == NULL) {
        LOG_DEBUG("[DB] Failed writting information to auction bid %s END file", aid);
        unlock_db_mutex("bid");
        return -1;
    }

    if (fputs(bid_info, fp) < 0) {
        LOG_DEBUG("[DB] Failed writting information to auction bid %s END file", aid);
        fclose(fp);
        unlock_db_mutex("bid");
        return -1;
    }

    fclose(fp);

    int ufd;
    char user_bidded[32];
    sprintf(user_bidded, "USERS/%.6s/BIDDED/%.3s.txt", uid, aid);
    if ((ufd = open(user_bidded, O_CREAT, SERVER_MODE)) < 0) {
        LOG_DEBUG("[DB] Failed creating bid file for user %s on auction %s", uid, aid);
        unlock_db_mutex("bid");
        return -1;
    }

    if (close(fd) != 0) {
        LOG_DEBUG("[DB] Failed closing bid file %s, resources may be leaking", aid);
        LOG_DEBUG("close: %s", strerror(errno));
    }

    unlock_db_mutex("bid");
    return 0;
}

int get_user_bids(char *uid, char *response) {
    lock_db_mutex("get user bids");
    struct dirent **entries;

    char bids_dir[32];
    sprintf(bids_dir, "USERS/%.6s/BIDDED", uid);
    int n_entries = scandir(bids_dir, &entries, NULL, alphasort);
    if (n_entries < 0) {
        LOG_DEBUG("[DB] Failed retrieving user auctions");
        LOG_DEBUG("[DB] scandir: %s", strerror(errno));
        unlock_db_mutex("get user bids");
        return -1;
    }
    
    int written = 0;
    char curr_auc_path[32];
    int check_fd;
    for (int i = 0; i < n_entries; i++) {
        if (entries[i]->d_name[0] == '.') continue;

        sprintf(curr_auc_path, "AUCTIONS/%.3s/END_%.3s.txt", entries[i]->d_name, entries[i]->d_name);
        char auc_status[8];
        if ((check_fd = open(curr_auc_path, 0, SERVER_MODE)) < 0) {
            sprintf(auc_status, " %.3s 1", entries[i]->d_name); // not ended
        } else {
            sprintf(auc_status, " %.3s 0", entries[i]->d_name); // ended
            if (close(check_fd) != 0) {
                LOG_DEBUG("[DB] Failed closing file descriptor, resources might be leaking")
                LOG_DEBUG("[DB] closedir: %s", strerror(errno));
            };
        }

        strcat(response, auc_status);
        written += strlen(auc_status);

        free(entries[i]);
    }

    free(entries);

    strcat(response, "\n");
    written += 1;

    unlock_db_mutex("get user bids");
    return written;
}

int lock_db_mutex(char *resource) {
    if (pthread_mutex_lock(&db_mutex) != 0) {
        LOG_DEBUG("[DB] Failed locking mutex for resource %s", resource);
        LOG_ERROR("Failed pthread_mutex_lock, fatal...");
        exit(1);
    }    
    return 0;
}

int unlock_db_mutex(char *resource) {
    if (pthread_mutex_unlock(&db_mutex) != 0) {
        LOG_DEBUG("[DB] Failed unlocking mutex for resource %s", resource);
        LOG_ERROR("Failed pthread_mutex_lock, fatal...");
        exit(1);
    }    
    return 0;
}