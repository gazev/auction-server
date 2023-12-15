#ifndef __CONFIG_H__
#define __CONFIG_H__

#define DEFAULT_PORT "58078" // default port where AS will be ran 

/**
* Server configurations
*/
#define DB_ROOT "ASDIR" // database root directory name

#define THREAD_POOL_SZ 30 // number of TCP worker threads (also max number of TCP connections allowed)

#define TCP_SERV_TIMEOUT 5 // in seconds
#define UDP_SERV_TIMEOUT 5  // in seconds

/**
* Client configuration
*/
#define TCP_CLIENT_TIMEOUT 5 // in seconds
#define UDP_CLIENT_TIMEOUT 5 // in seconds

#endif