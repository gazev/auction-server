#include <stdlib.h>
#include <arpa/inet.h>

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