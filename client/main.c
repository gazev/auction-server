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

/**
Print program's help message
*/
void print_usage() {
    char *usage_str =  
    "usage: ./user [-h] [-n ASIP] [-p ASport]\n\n"

    "options:\n"
    "  -h,        show this message and exit\n"
    "  -n ASIP,   IP of AS server\n"
    "  -p ASPORT, port where the server will be listening on (default: 600009)\n";

    fprintf(stdout, "%s", usage_str);
}


int main(int argc, char **argv) { 
    // arguments we want to determine 
    char *port = DEFAULT_PORT;
    char *ip   = NULL;

    // parse input arguments with getopt()
    int opt = 0; 
    while ((opt = getopt(argc, argv, "hn:p:")) != -1) { 
        switch (opt) {
            case 'h':
                print_usage();
                exit(0);

            case 'n':
                if (is_valid_ip_addr(optarg)) {
                    ip = optarg;
                } else {
                    LOG_WARN("Ignoring invalid -n argument: %s", optarg);
                }
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

    // if no IP is provided, we assume it's running on the local machine
   if (!ip) {
        struct addrinfo hints = {
            .ai_family = AF_INET,
            .ai_socktype = SOCK_DGRAM,
            .ai_flags = 0,
        };
        struct addrinfo *addr;

        int err = getaddrinfo(NULL, port, &hints, &addr);
        if (err) {
            if (err == EAI_SYSTEM) {
                LOG_ERROR("getaddrinfo: %s", strerror(errno));
            } else {
                LOG_ERROR("getaddrinfo: %s", gai_strerror(err));
            }
            exit(1);
        }

        // getting the IP
        ip = inet_ntoa(((struct sockaddr_in *)addr->ai_addr)->sin_addr);

        freeaddrinfo(addr);
    }
    LOG("%s", ip);

    // call the real func now like
    // client(ip, port);

    return 0;
}