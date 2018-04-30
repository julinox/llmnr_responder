/** ***************************************************
 * Interface to handle all related to socket creation *
 * and sockets setting                                *
 ******************************************************/

#ifndef LLMNR_SOCKETS_H
#define LLMNR_SOCKETS_H

typedef struct {
        U_CHAR id;
        int socket;
        int iptype;
        int recviface;
        SA_STORAGE from;
        U_CHAR sndPkt[SNDBUFSZ];
        U_CHAR ancBuffer[ANCBUFSZ];
        U_CHAR rcvBuffer[RCVBUFSZ];
        struct in_pktinfo *pktinfo4;
        struct in6_pktinfo *pktinfo6;
} UDPCLIENT;

typedef struct {
        int socket;
        int ipType;
        int recvIface;
        SA_IN6 from;
        U_CHAR sndPkt[TCPBUFFSZ];
        U_CHAR rcvBuffer[RCVBUFSZ];
} TCPCLIENT;

PUBLIC int createNetLinkSocket();
PUBLIC int createUdpSocket(int family);
PUBLIC int createTcpSock();
PUBLIC int createC4Sock(INADDR *ip);
PUBLIC int createC6Sock(int ifIndex);
PUBLIC int getPktInfo(int family, struct msghdr *msg, UDPCLIENT *client);
PUBLIC int __getPktInfo(int family, void *ip, struct msghdr *msg);
PUBLIC void joinMcastGroup(POLLFD *polling, NETIFACE *iface);
PUBLIC void leaveMcastGroup(POLLFD *polling, NETIFACE *iface, INADDR *copy);
PUBLIC void getTcpPktInfo(SA_IN6 *name, TCPCLIENT *client, NETIFACE *ifaces);
PUBLIC void fillMcastVars();

#endif
