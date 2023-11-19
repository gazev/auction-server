#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <errno.h>

#include "../utils/logging.h"
#include "../utils/validators.h"
#include "../utils/consts.h"

#include "client.h"
#include "command_table.h"
#include "parser.h"
/**
Print program's help message
*/
void print_usage() {
    char *usage_fmt =  
    "usage: ./user [-h] [-n ASIP] [-p ASport]\n\n"

    "options:\n"
    "  -h,        show this message and exit\n"
    "  -n ASIP,   IP of AS server\n"
    "  -p ASPORT, port where the server will be listening on (default: %s)\n";

    fprintf(stdout, usage_fmt, DEFAULT_PORT);
}


int main(int argc, char **argv) { 
    log_level_t g_level = LOG_NORMAL;

    // arguments we want to determine 
    char *port = DEFAULT_PORT;
    char *ip   = NULL;

    // parse input arguments with getopt()
    int opt = 0; 
    while ((opt = getopt(argc, argv, "hdn:p:")) != -1) { 
        switch (opt) {
            case 'h':
                print_usage();
                exit(0);

            case 'n':
                if (!is_valid_ip_addr(optarg)) {
                    LOG_WARN("Invalid IP literal (contiuing assuming IP is a domain): %s", optarg);
                }
                ip = optarg;
            
            case 'd': // set debug
                g_level = LOG_DEBUG;
                break;

            case 'p':
                if (is_valid_port(optarg)) {
                    port = optarg;
                } else {
                    LOG_WARN("Ignoring invalid -p argument: %s", optarg);
                }
                break;

            default:
                print_usage();
                exit(1);
        }
    }

    set_log_level(g_level);
    run_client(ip, port);

    return 0;
}