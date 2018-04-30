/** ********************************************
 * Some general macros definitions (Those that *
 * are used by several files                   *
 ***********************************************/

#ifndef LLMNR_DEFS_H
#define LLMNR_DEFS_H

#define PUBLIC
#define PRIVATE static
#define U_CHAR unsigned char
#define U_SHORT unsigned short
#define SA struct sockaddr
#define SA_IN struct sockaddr_in
#define SA_IN6 struct sockaddr_in6
#define SA_STORAGE struct sockaddr_storage
#define INADDR struct in_addr
#define IN6ADDR struct in6_addr
#define POLLFD struct pollfd

#define TRUE 1
#define FALSE 0
#define SUCCESS 0
#define FAILURE -1
#define SYSFAILURE 1
#define IPV4LEN 4
#define IPV6LEN 16
#define HEADSZ 12
#define MAXIFACES 16
#define LLMNRPORT 5355
#define HOSTNAMEMAX 255
#define LLMNR_TIMEOUT 150
#define JITTER_INTERVAL 100

#define SNDBUFSZ 150
#define RCVBUFSZ 512
#define ANCBUFSZ 256
#define TCPBUFFSZ 2048
#endif
