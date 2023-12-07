#ifndef __UDP_H__ 
#define __UDP_H__

#include <stdlib.h>

#define UDP_ERR_NO_LF_MESSAGE 2

int send_udp_request(char *buff, size_t size, struct client_state *);
int receive_udp_response(char *buff, size_t size, struct client_state *);
 
#endif