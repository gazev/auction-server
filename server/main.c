#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../utils/logging.h"
#include "../utils/validators.h"
#include "../utils/config.h"

#include "server.h"

/**
Print program's help message
*/
void print_usage() {
    char *usage_fmt = 
        "usage: ./AS [-h] [-v] [-p ASport]\n\n"

        "options:\n"
        "  -h,        show this message and exit\n"
        "  -v,        set log level to verbose\n"
        "  -d,        set log level to debug\n"
        "  -p ASport, port where the server will be listening (default: %s)\n";

    fprintf(stdout, usage_fmt,  DEFAULT_PORT);
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
                if (!g_level) { // don't overwrite debug option if it is set
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
                // this is an error
                print_usage();
                exit(1);
        }
    }

    set_log_level(g_level);

    // call server(port) here
    server(port);
    return 0;
}