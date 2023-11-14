#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../utils/logging.h"
#include "../utils/validators.h"
#include "../utils/consts.h"

/**
Print program's help message
*/
void print_usage() {
    char *usage_str =  
    "usage: ./AS [-h] [-v] [-p ASport]\n\n"

    "options:\n"
    "  -h,        show this message and exit\n"
    "  -v,        set log level to verbose\n"
    "  -d,        set log level to debug\n"
    "  -p ASport, port where the server will be listening (default: 600009)\n";

    fprintf(stdout, "%s", usage_str);
}


int main(int argc, char **argv) { 
    // arguments we want to determine
    log_level_t g_level = LOG_NORMAL;
    char *port = DEFAULT_PORT;

    int opt = 0; 
    while ((opt = getopt(argc, argv, "hdvp:")) != -1) { 
        switch (opt) {
            case 'h':
                print_usage();
                exit(0);

            case 'v': // set verbose
                if (!g_level) { // don't overwrite debug option if set
                    g_level = LOG_VERBOSE;
                }
                break;

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

    return 0;
}