#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>

#include "logging.h"
#include "utils.h"

/**
* Utility functions common for both client and server 
*/

/** Max file size is 10MB, so we could actually just read it to a stack buffer of that size,
+ but that will limit the amount of threads, even though they are fairly low */
#define BUFF_SZ 65536 // 64 KiB

/**
* Receive an asset file over a TCP connection with the AS protocol (expects LF terminating message).
* Returns 0 on success and an error code on error.
*
* Errors:
* 1 on error reading from connection socket
* 2 on error writting to asset file 
* 3 on invalid protocol
*/
int as_recv_asset_file(int afd, int conn_fd, int fsize) {
    char buff[BUFF_SZ];
    int total_read = 0;
    ssize_t read = 0;

    /**
    * We will start downloading confident that the client will not send an invalid
    * message by the end, if it does this action must be rolled back!
    */
    while (1) {
        read = recv(conn_fd, buff, BUFF_SZ, 0);
        if (read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                LOG_VERBOSE("[RECV ASSET] Timed out waiting for data");
                LOG_DEBUG("[RECV ASSET] recv: %s", strerror(errno));
            } else {
                LOG_VERBOSE("[RECV ASSET] Failed reading from connection socket");
                LOG_DEBUG("[RECV ASSET] recv: %s", strerror(errno));
            }
            return ERR_TCP_READ;
        }

        if (read == 0) {
            LOG_VERBOSE("[RECV ASSET] Connection was closed");
            return ERR_TCP_READ_CLOSED;
        }

        total_read += read;
        if (total_read > fsize)
            break;

        int written_to_file = 0;
        char *ptr = buff;
        do {
            ssize_t written = write(afd, ptr + written_to_file, read - written_to_file);
            if (written <= 0) {
                LOG_DEBUG("[RECV ASSET] Failed writting to asset file");
                LOG_DEBUG("[RECV ASSET] write: %s", strerror(errno));
                return ERR_WRITE_AF;
            }
            written_to_file += written;
            ptr += written;
        } while (written_to_file < read);
    }

    // if more than what was announced was sent it is an error
    if (total_read > fsize + 1) {
        LOG_VERBOSE("[RECV ASSET] Got an invalid protocol message")
        return ERR_INVALID_PROTOCOL;
    }

    // check if LF finalizes the message, if it doesn't it is breaking the protocol
    if (buff[read - 1] != '\n') {
        LOG_VERBOSE("[RECV ASSET] Got an invalid protocol message")
        return ERR_INVALID_PROTOCOL; 
    }

    /**
    * Here we write the last block. We do it outside of the loop because we
    * must first determine if \n was received and strip it
    */    
    int last_block_written = 0;
    char *ptr = buff;
    while (last_block_written < read - 1) {
        ssize_t written = write(afd, ptr + last_block_written, read - last_block_written - 1);
        if (written <= 0) {
            LOG_VERBOSE("[RECV ASSET] Failed writting last block to asset file");
            LOG_DEBUG("[RECV ASSET] write: %s", strerror(errno));
            return ERR_WRITE_AF;
        }

        last_block_written += written;
        ptr += written;
    }

    return 0;
}

/**
* Receive na asset file over a TCP connection with the AS protocol (checks \n).
* Returns 0 on success and an error code on error.
*
* Errors:
* 1 on error reading from asset file 
* 2 if file is not of size fsize 
* 3 on error writting from socket
*/
int as_send_asset_file(int afd, int conn_fd, int fsize) {
    char buff[BUFF_SZ];
    int total_read = 0;
    while (total_read < fsize) {
        ssize_t n = read(afd, buff, BUFF_SZ);
        if (n < 0) {
            LOG_VERBOSE("[SEND ASSET] Failed reading data asset file");
            LOG_DEBUG("[SEND ASSET] read: %s", strerror(errno));
            return ERR_READ_AF;
        }

        if (n == 0) {
            LOG_DEBUG("[SEND ASSET] Invalid fsize specified");
            return ERR_BAD_FSIZE;
        }
        
        total_read += n;

        int read_sent = 0;
        char *ptr = buff;
        do {
            ssize_t sent = send(conn_fd, ptr + read_sent, n - read_sent, 0);
            if (sent < 0) {
                if (errno == EPIPE) {
                    LOG_VERBOSE("[SEND ASSET] Connection closed while sending asset data");
                } else {
                    LOG_VERBOSE("[SEND ASSET] Failed sending asset data");
                    LOG_DEBUG("[SEND ASSET] send: %s", strerror(errno));
                }
                return ERR_TCP_WRITE;
            }
            read_sent += sent;
        } while(read_sent < n);
    }

    /**
    * Send terminating LF
    */
    if (send_tcp_message("\n", 1, conn_fd) != 0) {
        LOG_VERBOSE("[SEND ASSET] Failed sending terminating LF");
        return ERR_TCP_WRITE;
    }

    return 0;
}


/**
* Read from `conn_fd` stream connection `n` bytes into `buff`.
* Return 0 on success and 1 on error. 
*/
int read_tcp_stream(char *buff, int n, int conn_fd) {
    char *ptr = buff;
    int total_read = 0;
    while (total_read < n) {
        ssize_t read = recv(conn_fd, ptr + total_read, n - total_read, 0);
        if (read < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                LOG_DEBUG("Timed out receiving response");
                LOG_DEBUG("recv: %s", strerror(errno));
            } else {
                LOG_DEBUG("Failed reading from socket");
                LOG_DEBUG("recv: %s", strerror(errno));
            }
            return ERR_TCP_READ;
        }

        if (read == 0) {
            LOG_DEBUG("Connection closed by client");
            return ERR_TCP_READ_CLOSED;
        }

        total_read += read;
        ptr += read;
    }

    return 0;
}

/**
* Write into `conn_fd` stream connection `n` bytes from `message`.
* Returns 0 on success and -1 on error
*/
int send_tcp_message(char *message, int n, int conn_fd) {
    char *ptr = message;
    int total_sent = 0;
    while (total_sent < n) {
        ssize_t written = send(conn_fd, ptr + total_sent, n - total_sent, 0);
        if (written < 0) {
            if (errno == EPIPE) {
                LOG_DEBUG("Connection closed");
            } else {
                LOG_DEBUG("send: %s", strerror(errno));
                LOG_DEBUG("Failed sending response");
            }
            return ERR_TCP_WRITE;
        }

        total_sent += written;
        ptr += written;
    }

    return 0;
}


/**
* Read a byte from `conn_fd` stream to assert if it's the LF character, aka, the 
* terminating character of the protocol.
* This function will consume the byte from the stream so it should only be 
* used to check for \n at the end of messages.
*/
int is_lf_in_stream(int conn_fd) {
    char terminating_byte = '\0';

    if (recv(conn_fd, &terminating_byte, 1, 0) < 0) {
        return 0;
    }

    return terminating_byte == '\n';
}
