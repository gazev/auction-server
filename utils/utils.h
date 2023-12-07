#ifndef __UTILS_H__
#define __UTILS_H__

#define ERR_TCP_READ 1
#define ERR_INVALID_PROTOCOL 2
#define ERR_WRITE_AF 3

#define ERR_TCP_WRITE 1
#define ERR_READ_AF 2
#define ERR_BAD_FSIZE 3

#define ERR_TCP_READ_CLOSED 2

int as_send_asset_file(int afd, int conn_fd, int fsize);
int as_recv_asset_file(int afd, int conn_fd, int fsize);

int read_tcp_stream(char *buff, int n, int conn_fd);
int send_tcp_message(char *message, int n, int conn_fd);
int is_lf_in_stream(int conn_fd);

#endif