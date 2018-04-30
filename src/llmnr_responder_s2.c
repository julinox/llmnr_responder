/* Macros */
#define BACKLOG 10
#define _GNU_SOURCE
#define POLLINGSZ 4
#define MAXWAITING 5
#define NLBUFSZ 1024
#define QUESTMINSZ 17
#define NLTIMEOUT 20

/* Includes */
#include <poll.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <linux/if_arp.h>
#include <linux/rtnetlink.h>

/* Own includes */
#include "../include/llmnr_defs.h"
#include "../include/llmnr_names.h"
#include "../include/llmnr_net_interface.h"
#include "../include/llmnr_rr.h"
#include "../include/llmnr_print.h"
#include "../include/llmnr_utils.h"
#include "../include/llmnr_str_list.h"
#include "../include/llmnr_syslog.h"
#include "../include/llmnr_signals.h"
#include "../include/llmnr_packet.h"
#include "../include/llmnr_sockets.h"
#include "../include/llmnr_conflict.h"
#include "../include/llmnr_conflict_list.h"
#include "../include/llmnr_responder_s2.h"

/* Enums & Structs */
enum _CONDITION {
        _WAIT,
        _RESUME,
        _CDARINIT,
        _CDARIFACEUP,
        _CDARCONFLICT,
};

typedef struct {
        NAME *cName;
        NETIFACE *cIface;
} CDARCONFLICT;

/* Private prototypes */
PRIVATE void start();
PRIVATE void initialJoin(POLLFD *polling);
PRIVATE void setDescriptorToPoll(POLLFD *pollArr, int i, int fd);
PRIVATE void removeDescriptorFromPoll(POLLFD *pollArr, int i);
PRIVATE void handleError(int err, POLLFD *pollArr, int i);
PRIVATE void handleUdpQuery(int fd);
PRIVATE void handleTcpQuery(int fd);
PRIVATE void *handleUdpWorker(void *clientData);
PRIVATE void *handleTcpWorker(void *clientData);
PRIVATE void checkConflicts();
PRIVATE void _checkLinkLocalAddr(char *ipv6, char *_buff);
PRIVATE void checkIfDown(NETIFACE *iface);
PRIVATE void invokeCdar(U_CHAR type, int ifIndex, NAME *name);
PRIVATE void *initialDefense(void *__);
PRIVATE void *ifaceUpDefense(void *ifIndex);
PRIVATE void *conflictDefense(void *cdarCond);
PRIVATE void handleNetlinkQuery(POLLFD *polling, NETIFACE *ifaces);
PRIVATE void getRta(struct nlmsghdr *nlMsg, POLLFD *polling, NETIFACE *ifaces);
PRIVATE void addAddr (POLLFD *polling, NETIFACE *ifaces, int index, int fam, void *addr);
PRIVATE void delAddr (POLLFD *polling, NETIFACE *ifaces, int index, int fam, void *addr);
PRIVATE void sigTermHandler(int signo);
PRIVATE void sigUsr2Handler(int signo);
PRIVATE void freeResources(POLLFD *pollArr);
PRIVATE int checkName(char *name, int ifIndex, U_CHAR *T);
PRIVATE int checkPtrName(char *name, int ifIndex, int family);
PRIVATE int _checkPtrName(char *name, NETIFIPV4 *ipv4s);
PRIVATE int __checkPtrName(char *name, NETIFIPV6 *ipv6s);
PRIVATE int checkLinkLocalAddr(char *name);
PRIVATE U_CHAR threadsRunning();

/* Glocal variables */
int Flag;
PRIVATE NAME *Names;
PRIVATE RRLIST *Rlist;
PRIVATE NETIFACE *Ifaces;
PRIVATE CONFLICT *Conflicts;
PRIVATE pthread_t MainTid;
PRIVATE U_CHAR RunningCDAR;
PRIVATE volatile int ThreadsCount;
PRIVATE pthread_mutex_t CountMutex;
PRIVATE pthread_mutex_t ConflictMutex;

/* Functions definitions */
PUBLIC void startS2(NAME *_N, NETIFACE *_I, RRLIST *_R, CONFLICT *_C)
{
    /*
     * Set Glocal variables
     */
        pthread_mutexattr_t attr;

        Flag = 1;
        ThreadsCount = 0;
        RunningCDAR = 0;
        Names = _N;
        Ifaces = _I;
        Rlist = _R;
        Conflicts = _C;
        MainTid = pthread_self();
        srandom(time(NULL));

        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
        if (pthread_mutex_init(&CountMutex,&attr))
                pthread_mutex_init(&CountMutex,NULL);
        if (pthread_mutex_init(&ConflictMutex,&attr))
                pthread_mutex_init(&ConflictMutex,NULL);
        pthread_mutexattr_destroy(&attr);
        start();
}

PRIVATE void start()
{
    /*
     * - Create the sockets and set them to be polled
     * - Set the signal handler
     * - Invoke the initial cdar process for every interface
     * - Check if any conflict pending
     * - Poll the sockets
     * Note: Is necessary to wait all running threads to be
     * done before launch the netlink socket handler (for
     * data structures consistency)
     */
        struct pollfd polling[POLLINGSZ];
        int i, udpSock4, udpSock6, tcpSock, netLinkSock;

        udpSock4 = 0;
        udpSock6 = 0;
        tcpSock = 0;
        netLinkSock = 0;
        memset(polling,0,sizeof(struct pollfd));
        memset(polling + 1,0,sizeof(struct pollfd));
        memset(polling + 2,0,sizeof(struct pollfd));
        memset(polling + 3,0,sizeof(struct pollfd));
        udpSock4 = createUdpSocket(AF_INET);
        udpSock6 = createUdpSocket(AF_INET6);
        tcpSock = createTcpSock();
        netLinkSock = createNetLinkSocket();
        setDescriptorToPoll(polling,0,udpSock4);
        setDescriptorToPoll(polling,1,udpSock6);
        setDescriptorToPoll(polling,2,tcpSock);
        setDescriptorToPoll(polling,3,netLinkSock);
        fillMcastVars();
        handleSignals();
        handleSpecSignals(SIGTERM,sigTermHandler);
        handleSpecSignals(SIGUSR2,sigUsr2Handler);
        initialJoin(polling);
        invokeCdar(_CDARINIT,0,NULL);

        while (Flag) {
                checkConflicts();
                if (poll(polling,POLLINGSZ,-1) < 0) {
                        if (errno == EINTR) {
                                checkConflicts();
                                continue;
                        }
                }
                for (i=0; i < POLLINGSZ; i++) {
                        if (polling[i].revents & POLLIN) {
                                if (i == 0 || i == 1) {
                                        handleUdpQuery(polling[i].fd);
                                } else if (i == 2) {
                                        handleTcpQuery(polling[i].fd);
                                } else if (i == 3) {
                                    if (!threadsRunning())
                                            handleNetlinkQuery(polling,Ifaces);
                                }

                        } else if (polling[i].revents & POLLERR) {
                                handleError(EPOLLERR,polling,i);

                        } else if (polling[i].revents & POLLHUP) {
                                handleError(EPOLLERR,polling,i);
                        }
                }
        }
        freeResources(polling);
}

PRIVATE void initialJoin(POLLFD *polling)
{
    /*
     * For every LLMNR interface do the initial multicast join
     */
        NETIFACE *current;

        if (Ifaces == NULL)
                return;
        current = Ifaces;
        for (; current != NULL; current = current->next)
                joinMcastGroup(polling,current);
}

PRIVATE void invokeCdar(U_CHAR type, int ifIndex, NAME *name)
{
    /*
     * Cdar = Conflict detection and resolution
     * Cdar process is invoke in 3 cases:
     * - When the daemon starts and every LLMNR interface
     *   must query his own hostname (s) (this happens only one time)
     * - When an interface goes up
     * - When a query with the 'CONFLICT' flag is received
     * Even though the cdar process is done by a separated thread the
     * 'RunningCDAR' flag is used to ensure that one and only
     * one cdar process thread is running (for data structures
     * consistency)
     */
        int *index;
        pthread_t tid;
        NETIFACE *iface;
        pthread_attr_t detach;
        CDARCONFLICT *cdarCond;

        pthread_attr_init(&detach);
        pthread_attr_setdetachstate(&detach,PTHREAD_CREATE_DETACHED);

        if (type == _CDARINIT) {
                pthread_create(&tid,&detach,initialDefense,NULL);

        } else if (type == _CDARIFACEUP) {
                index = calloc(1,sizeof(int));
                if (index == NULL)
                        return;
                *index = ifIndex;
                if (pthread_create(&tid,&detach,ifaceUpDefense,(void *)index))
                        free(index);

        } else if (type == _CDARCONFLICT) {
                iface = getNetIfNodeByIndex(Ifaces,ifIndex);
                if (iface == NULL)
                        return;
                cdarCond = calloc(1,sizeof(CDARCONFLICT));
                if (cdarCond == NULL)
                        return;
                cdarCond->cName = name;
                cdarCond->cIface = iface;
                if (pthread_create(&tid,&detach,conflictDefense,
                                   (void *)cdarCond))
                        free(cdarCond);
        }
}

PRIVATE void *initialDefense(void *__)
{
    /*
     * For every interface, do the inital conflict detection
     * This happens only one time
     */
        NAME *cName;
        NETIFACE *cIface;

        __ =__;
        if (Names == NULL || Ifaces == NULL)
                return NULL;
        cName = Names;
        for (; cName != NULL; cName = cName->next) {
                if (cName->name == NULL)
                        continue;
                for (cIface = Ifaces; cIface != NULL; cIface = cIface->next) {
                        if (cIface->flags & _IFF_NOIF)
                                continue;
                        if (!(cIface->flags & _IFF_RUNNING))
                                continue;
                        cDar(cName,cIface,Ifaces);
                }
        }
        return NULL;
}

PRIVATE void *ifaceUpDefense(void *ifIndex)
{
    /*
     * Conflict detection when an interface goes up
     * Important to mention that the process that
     * handles the interfaces changes can only be
     * run when theres no other threads running
     */
        int index;
        NAME *cName;
        U_CHAR flag;
        NETIFACE *iface;

        flag = 0;
        cName = Names;
        index = *((int *)ifIndex);
        iface = getNetIfNodeByIndex(Ifaces,index);
        for (; cName != NULL; cName = cName->next) {
                if (cName->name == NULL)
                        continue;
                cDar(cName,iface,Ifaces);
        }
        free(ifIndex);
        RunningCDAR = 0;
        pthread_mutex_lock(&ConflictMutex);
        if (countConflicts(Conflicts) > 1)
                flag = 1;
        pthread_mutex_unlock(&ConflictMutex);
        if (flag)
                sendSignal(MainTid);
        return NULL;
}

PRIVATE void *conflictDefense(void *cdarCond)
{
    /*
     * Conflict detection and resolution when a
     * conflict query received.
     */
        U_CHAR flag;
        CDARCONFLICT *cdarC;

        flag = 0;
        if (Names == NULL || Ifaces == NULL)
                return NULL;
        cdarC = (CDARCONFLICT *)cdarCond;
        cDar(cdarC->cName,cdarC->cIface,Ifaces);
        free(cdarC);
        RunningCDAR = 0;
        pthread_mutex_lock(&ConflictMutex);
        if (countConflicts(Conflicts) > 1)
                flag = 1;
        pthread_mutex_unlock(&ConflictMutex);
        if (flag)
                sendSignal(MainTid);
        return NULL;
}

PRIVATE void handleUdpQuery(int fd)
{
    /*
     * Receive the query using recvmsg() and
     * start a thread to attend the petition.
     * recvmsg() is used because is crucial to know
     * which interface received the query. Also when
     * the thread is started a mutex is used to update
     * a variable that keeps the count of the running
     * threads
     */
        pthread_t tid;
        UDPCLIENT *client;
        NETIFACE *iface;
        struct iovec iov;
        struct msghdr msg;
        pthread_attr_t detach;
        char auxBuffer[RCVBUFSZ];

        memset(&client,0,sizeof(client));
        client = calloc(1,sizeof(UDPCLIENT));
        if (client == NULL) {
                recvfrom(fd,auxBuffer,RCVBUFSZ,0,NULL,NULL);
                return;
        }
        iov.iov_base = client->rcvBuffer;
        iov.iov_len = RCVBUFSZ;
        msg.msg_name = &client->from;
        msg.msg_namelen = sizeof(SA_STORAGE);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = client->ancBuffer;
        msg.msg_controllen = ANCBUFSZ;
        msg.msg_flags = 0;
        if (recvmsg(fd,&msg,0) < QUESTMINSZ) {
                free(client);
                return;
        }
        client->id = (U_CHAR)random();
        client->socket = fd;
        client->pktinfo4 = NULL;
        client->pktinfo6 = NULL;
        if (getPktInfo(client->from.ss_family,&msg,client)) {
                free(client);
                return;
        }
        iface = getNetIfNodeByIndex(Ifaces,client->recviface);
        if (iface == NULL) {
                free(client);
                return;
        }
        pthread_attr_init(&detach);
        pthread_attr_setdetachstate(&detach,PTHREAD_CREATE_DETACHED);
        pthread_mutex_lock(&CountMutex);
        if (pthread_create(&tid,&detach,handleUdpWorker,client))
                free(client);
        else
                ThreadsCount++;
        pthread_mutex_unlock(&CountMutex);
}

PRIVATE void *handleUdpWorker(void *clientData)
{
    /*
     * Responds the query. Check a bunch of stuff
     * about the header and the query itself. When
     * done decrement the 'ThreadsCount' variable.
     * If the query is a conflict alarm then add it
     * to the 'CONFLICT' list and send a signal to
     * the main thread to notify the conflict
     * On normal query, head is reused
     */
        int pktSz;
        NAME *aux;
        HEADER head;
        QUERY query;
        PKTSND pktSnd;
        DSTRUCTURE dsts;
        PKTPARAMS params;
        UDPCLIENT *client;
        U_CHAR namePtr[2];

        if (clientData == NULL)
                return NULL;
        client = (UDPCLIENT *)clientData;
        memset(&head,0,sizeof(head));
        memset(&query,0,sizeof(query));
        getHeader(client->rcvBuffer,&head);

        if (head.QR != 0 || head.OPCODE != 0 || head.QDCOUNT != 1 ||
            head.ANCOUNT != 0 || head.NSCOUNT != 0)
                goto CleanHUW;
        getQuery(client->rcvBuffer,&query);
        if (query.QTYPE == PTR) {
                if (checkPtrName(query.QNAME,client->recviface,
                                 client->from.ss_family))
                        goto CleanHUW;
                head.T = 0;
        } else {
                if (checkName(query.QNAME,client->recviface,&head.T))
                        goto CleanHUW;
                if (head.C == 1) {
                        aux = getNameNodeByName(query.QNAME,Names);
                        if (aux == NULL)
                                goto CleanHUW;
                        pthread_mutex_lock(&ConflictMutex);
                        addConflict(aux,query.QTYPE,&client->from,client->recviface,
                                    Conflicts);
                        pthread_mutex_unlock(&ConflictMutex);
                        sendSignal(MainTid);
                        goto CleanHUW;
                }
        }
        namePtr[0] = 0xC0;
        namePtr[1] = HEADSZ;
        params.head = &head;
        params.query = &query;
        params.ipType = client->iptype;
        params.rcvIface = client->recviface;
        params.pktBuff = client->sndPkt;
        params.pktBuffSz = SNDBUFSZ;
        params.namePtr = (U_SHORT *)namePtr;
        dsts.names = Names;
        dsts.ifaces = Ifaces;
        dsts.rList = Rlist;
        pktSz = attachAnswer(&params,&dsts);
        pktSnd.fd = client->socket;
        pktSnd.ifIndex = client->recviface;
        pktSnd.pktBuff = client->sndPkt;
        pktSnd.pktSz = pktSz;
        pktSnd.to = (SA *)&client->from;
        if (head.T != 0)
            poll(0,0,random() % JITTER_INTERVAL);
        sendUDPacket(&pktSnd);

        CleanHUW:
        free(client);
        pthread_mutex_lock(&CountMutex);
        ThreadsCount--;
        if (ThreadsCount <= 0) {
                if (ThreadsCount < 0)
                    ThreadsCount = 0;
        }
        pthread_mutex_unlock(&CountMutex);
        return NULL;
}

PRIVATE void handleTcpQuery(int fd)
{
    /*
     * Same thing that does handleUdpQuery() but
     * this time for TCP. Use of getsockname() (instead
     * of recvmsg) to know the interface that received
     * the query
     */
        SA_IN6 name;
        pthread_t tid;
        socklen_t fromLen;
        TCPCLIENT *client;
        pthread_attr_t detach;

        fromLen = sizeof(SA_IN6);
        client = calloc(1,sizeof(TCPCLIENT));
        if (client == NULL) {
                accept(fd,NULL,NULL);
                return;
        }
        memset(&name,0,sizeof(SA_IN6));
        client->socket = accept(fd,(SA *)&client->from,&fromLen);
        fromLen = sizeof(SA_IN6);
        if (getsockname(client->socket,(SA *)&name,&fromLen) < 0) {
                free(client);
                return;
        }
        getTcpPktInfo(&name,client,Ifaces);
        if (client->recvIface == 0) {
                free(client);
                return;
        }
        pthread_attr_init(&detach);
        pthread_attr_setdetachstate(&detach,PTHREAD_CREATE_DETACHED);
        pthread_mutex_lock(&CountMutex);
        if (pthread_create(&tid,&detach,handleTcpWorker,(void *)client))
                free(client);
        else
                ThreadsCount++;
        pthread_mutex_unlock(&CountMutex);
}

PRIVATE void *handleTcpWorker(void *clientData)
{
    /*
     * Same thing that does handleUdpWorker() but this
     * time for TCP
     */
        int pktSz, rcved;
        HEADER head;
        QUERY query;
        PKTSND pktSnd;
        DSTRUCTURE dsts;
        PKTPARAMS params;
        TCPCLIENT *client;
        U_CHAR namePtr[2];

        if (clientData == NULL)
                return NULL;
        client = (TCPCLIENT *)clientData;
        if ((rcved = recv(client->socket,client->rcvBuffer,RCVBUFSZ,0)) < QUESTMINSZ)
                goto CleanHTW;
        memset(&head,0,sizeof(head));
        memset(&query,0,sizeof(query));
        getHeader(client->rcvBuffer,&head);
        if (head.QR != 0 || head.OPCODE != 0 || head.QDCOUNT != 1 ||
            head.ANCOUNT != 0 || head.NSCOUNT != 0 || head.C == 1)
                goto CleanHTW;
        getQuery(client->rcvBuffer,&query);
        if (query.QTYPE == PTR) {
                if (checkPtrName(query.QNAME,client->recvIface,AF_INET))
                        goto CleanHTW;
                head.T = 0;
        } else {
                if (checkName(query.QNAME,client->recvIface,&head.T))
                        goto CleanHTW;
        }
        namePtr[0] = 0xC0;
        namePtr[1] = HEADSZ;
        params.head = &head;
        params.query = &query;
        params.ipType = client->ipType;
        params.rcvIface = client->recvIface;
        params.pktBuff = client->sndPkt;
        params.pktBuffSz = TCPBUFFSZ;
        params.namePtr = (U_SHORT *)namePtr;
        dsts.names = Names;
        dsts.ifaces = Ifaces;
        dsts.rList = Rlist;
        pktSz = attachAnswer(&params,&dsts);

        pktSnd.fd = client->socket;
        pktSnd.ifIndex = client->recvIface;
        pktSnd.pktBuff = client->sndPkt;
        pktSnd.pktSz = pktSz;
        pktSnd.to = NULL;
        sendTCPacket(&pktSnd);

        CleanHTW:
        close(client->socket);
        free(client);
        pthread_mutex_lock(&CountMutex);
        ThreadsCount--;
        if (ThreadsCount <= 0) {
                if (ThreadsCount < 0)
                    ThreadsCount = 0;
        }
        pthread_mutex_unlock(&CountMutex);
        return NULL;
}

PRIVATE void handleNetlinkQuery(POLLFD *polling, NETIFACE *ifaces)
{
    /*
     * Process that handles network interfaces changes
     * Can only be run when there's no thread running (cdar,
     * handleUdpWorker() or handleTcpWorker())
     */
        int len;
        struct iovec iov;
        struct msghdr msg;
        struct nlmsghdr *nlhdr;
        unsigned char buff[NLBUFSZ];

        iov.iov_base = buff;
        iov.iov_len = NLBUFSZ;
        msg.msg_name = NULL;
        msg.msg_namelen = 0;
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
        msg.msg_flags = 0;
        len = recvmsg(polling[3].fd,&msg,0);
        if (len < 0)
                return;
        nlhdr = (struct nlmsghdr *)buff;
        for (; NLMSG_OK(nlhdr, len); nlhdr = NLMSG_NEXT(nlhdr,len)) {
                if (nlhdr->nlmsg_type == NLMSG_DONE)
                        break;
                else if (nlhdr->nlmsg_type == NLMSG_NOOP)
                        continue;
                else if (nlhdr->nlmsg_type == NLMSG_ERROR)
                        break;
                /* Future reference (maybe)
                 * if (nlhdr->nlmsg_type == RTM_GETLINK)
                 *      ;
                 * else if (nlhdr->nlmsg_type == RTM_DELLINK)
                 *      ;
                 */
                if (nlhdr->nlmsg_type == RTM_NEWADDR) {
                        getRta(nlhdr,polling,ifaces);
                } else if (nlhdr->nlmsg_type == RTM_DELADDR) {
                        getRta(nlhdr,polling,ifaces);
                }
        }
}

PRIVATE void getRta(struct nlmsghdr *nlMsg, POLLFD *polling, NETIFACE *ifaces)
{
    /*
     * Parse the netlink message (See NETLINK(3))
     */
        int index, msgType;
        struct in_addr *i4;
        struct in6_addr *i6;
        struct ifaddrmsg *ip;
        struct rtattr *attrs;
        int rtAttrLen, family;

        ip = (struct ifaddrmsg *) NLMSG_DATA(nlMsg);
        attrs = IFA_RTA(ip);
        rtAttrLen = IFA_PAYLOAD(nlMsg);
        family = ip->ifa_family;
        index = ip->ifa_index;
        msgType = nlMsg->nlmsg_type;

        for (; RTA_OK(attrs,rtAttrLen); attrs = RTA_NEXT(attrs,rtAttrLen)) {
                if (attrs->rta_type == IFA_ADDRESS) {
                        if (family == AF_INET) {
                                i4 = (struct in_addr*) RTA_DATA(attrs);
                                if (msgType == RTM_NEWADDR)
                                        addAddr(polling,ifaces,index,AF_INET,
                                               (void *)i4);
                                else if (msgType == RTM_DELADDR)
                                        delAddr(polling,ifaces,index,AF_INET,
                                                (void *)i4);
                        } else if (family == AF_INET6) {
                                i6 = (struct in6_addr*) RTA_DATA(attrs);
                                if (msgType == RTM_NEWADDR)
                                        addAddr(polling,ifaces,index,AF_INET6,
                                               (void *)i6);
                                else if (msgType == RTM_DELADDR)
                                        delAddr(polling,ifaces,index,AF_INET6,
                                            (void *)i6);
                        }
                }
        }
}

PRIVATE U_CHAR threadsRunning()
{
    /*
     * Checks if there is any thread running. If any
     * then wait 'LLMNR_TIMEOUT' milliseconds and try
     * again
     */
        U_CHAR ret;

        ret = TRUE;
        if (!RunningCDAR) {
                pthread_mutex_lock(&CountMutex);
                if (ThreadsCount <= 0) {
                        if (ThreadsCount < 0)
                                ThreadsCount = 0;
                        ret = FALSE;
                }
                pthread_mutex_unlock(&CountMutex);
        }
        if (ret)
		poll(0,0,NLTIMEOUT);
        return ret;
}

PRIVATE void addAddr (POLLFD *polling, NETIFACE *ifaces, int index, int fam, void *addr)
{
    /*
     * A new ip addres has been added to the interface. If the interface
     * does not exists (i.e the interface went up) then add a new
     * interface to the 'NETIFACE' list, join the interface to the LLMNR
     * multicast group and launch the 'cDar' process. If it does exists
     * then just add the new ip to his corresponding 'NETIFACE' node
     */
        INADDR *ip4;
        IN6ADDR *ip6;
        NETIFACE *iface;
        NETIFIPV4 *ip4Node;
        NETIFIPV6 *ip6Node;
        char ifName[IFNAMSIZ];

        if (ifaces == NULL)
                return;
        iface = NULL;
        _if_indexToName(index,ifName);
        iface = getNetIfNodeByIndex(ifaces,index);

        if (iface == NULL) {
                iface = getNetIfNodeByName(ifaces,ifName);
                if (iface != NULL)
                        iface->ifIndex = index;
        }
        if (iface == NULL) {
                if (ifaces->flags & _IFF_STATIC) {
                        return;
                } else {
                        if (addNetIfNode(ifName,0,_IFF_DYNAMIC,ifaces) < 0)
                                return;
                        iface = getNetIfNodeByIndex(ifaces,index);
                }
        }
        switch (fam) {
        case AF_INET:
                ip4 = (INADDR *)addr;
                if (addNetIfIPv4(ip4,iface) < 0)
                        return;
                ip4Node = getNetIfIpv4Node(iface,ip4);
                buildARecord(ip4,ip4Node->ARECORD,
                             ip4Node->PTR4RECORD);
                break;
        case AF_INET6:
                ip6 = (IN6ADDR *)addr;
                if (addNetIfIPv6(ip6,iface) < 0)
                        return;
                ip6Node = getNetIfIpv6Node(iface,ip6);
                buildAAAARecord(ip6,ip6Node->AAAARECORD,
                                ip6Node->PTR6RECORD);
                break;
        }
        joinMcastGroup(polling,iface);
        if (iface->flags & _IFF_CDAR)
                return;
        /*
         * For every ip that belongs to the "reporting" interface a
         * netlink message is generated by the kernel. So, before
         * launching the cdar process the '_INET6' flag is checked
         * (i.e the interface has at least one IPv6 in the 'NETIFACE'
         *  list). If the flag is down then check (at kernel level) if
         * the interface has an IPv6 (at least a link local address).
         * If it has it then dont launch cdar and wait for the next
         * netlink message
         */
        if (!(iface->flags & _IFF_INET6)) {
                if (!checkLinkLocalAddr(iface->name))
                        return;
        }
        RunningCDAR = 1;
        invokeCdar(_CDARIFACEUP,iface->ifIndex,NULL);
}

PRIVATE void delAddr (POLLFD *polling, NETIFACE *ifaces, int index, int fam, void *addr)
{
    /*
     * Remove an ip from the interface and leave (if necessary)
     * the multicast group.
     * Note: In linux, an interface goes down when it has no
     * ip assigned to it
     */
        INADDR copy;
        NETIFACE *iface;
        char ifName[IFNAMSIZ];

        iface = NULL;
        _if_indexToName(index,ifName);
        iface = getNetIfNodeByIndex(ifaces,index);
        if (iface == NULL) {
                iface = getNetIfNodeByName(ifaces,ifName);
                if (iface != NULL)
                        iface->ifIndex = index;
        }
        if (iface == NULL)
                return;
        switch (fam) {
        case AF_INET:
                memcpy(&copy,addr,sizeof(INADDR));
                remNetIfIPv4(iface,(INADDR *)addr);
                break;
        case AF_INET6:
                remNetIfIPv6(iface,(IN6ADDR *)addr);
                break;
        }
        leaveMcastGroup(polling,iface,&copy);
        checkIfDown(iface);
}

PRIVATE void checkIfDown(NETIFACE *iface)
{
    /*
     * If the interface goes down then turn off some flags
     * and update (if necessary) the mirror interfaces
     * (See llmnr_net_interface.h). Also update the 'NAME'
     * authorivative lists (See llmnr_names.h)
     */
        int i, sock;
        NAME *current;
        NETIFACE *aux;
        struct ifreq ifr;

        sock = socket(AF_INET,SOCK_DGRAM,0);
        memset(&ifr, 0, sizeof(ifr));
        if (iface->name == NULL)
                return;
        strcpy(ifr.ifr_name,iface->name);

        if (ioctl(sock, SIOCGIFFLAGS, &ifr) < 0)
                return;
        if (ifr.ifr_flags & IFF_RUNNING)
                return;
        iface->flags &= ~_IFF_CDAR;
        iface->flags &= ~_IFF_RUNNING;
        iface->flags &= ~_IFF_CONFLICT;
        for (current = Names; current != NULL; current = current->next) {
                if (current->name == NULL)
                        continue;
                if (current->authOnSz >= 1)
                        delAuthOn(current,iface->ifIndex);
                if (current->notAuthOnSz >= 1)
                        delNotAuthOn(current,iface->ifIndex);
        }
        for (i = 0; i < iface->mirrorIfSz; i++) {
                aux = getNetIfNodeByIndex(Ifaces,iface->mirrorIfs[i]);
                if (aux == NULL)
                        continue;
                delMirrorIf(aux,iface->ifIndex);
                delMirrorIf(iface,iface->mirrorIfs[i]);
                aux->flags &= ~_IFF_CONFLICT;
        }
}

PRIVATE int checkLinkLocalAddr(char *name)
{
    /*
     * Checks if this 'iface' has a link local ipv6 at
     * "kernel level" (See /proc/net/if_inet6 file)
     */
        char *ptr;
        FILE *file;
        IN6ADDR ipv6;
        STRNODE *list;
        char buff[55 + IFNAMSIZ];
        char path[] = "/proc/net/if_inet6";

        file = fopen(path,"r");
        if (file == NULL)
                return FAILURE;
        memset(&ipv6,0,sizeof(ipv6));
        while (fgets(buff,sizeof(buff),file) != NULL) {
                trim(buff,'\n');
                list = split(buff,32);
                ptr = getNodeAt(list,6);
                if (ptr != NULL) {
                        if (!strcasecmp(name,ptr)) {
                                ptr = getNodeAt(list,1);
                                _checkLinkLocalAddr(ptr,buff);
                                if (inet_pton(AF_INET6,buff,&(ipv6)) <= 0)
                                        return FAILURE;
                                if (ip6Type(&ipv6) == LINKLOCALIP) {
                                        fclose(file);
                                        deleteStrNodeList(&list);
                                        return SUCCESS;
                                }
                        }
                }
                deleteStrNodeList(&list);
        }
        fclose(file);
        return FAILURE;
}

PRIVATE void _checkLinkLocalAddr(char *ipv6, char *_buff)
{
    /*
     * checkLinkLocalAddr() helper. Fills the missing ":"
     * in ipv6 string
     */
        int i, j;
        char *ptr;
        char buff[55 + IFNAMSIZ];

        i = 0;
        j = 1;
        ptr = ipv6;
        memset(buff,0,sizeof(buff));
        while (*ptr) {
                buff[i++] = *ptr++;
                j++;
                if (j == 5) {
                        buff[i++] = ':';
                        j = 1;
                }
        }
        if (buff[i - 1] == ':')
                buff[i - 1] = 0;
        else if (buff[i] == ':')
                buff[i] = 0;
        memset(_buff,0,strlen(_buff));
        strcpy(_buff,buff);
}

PRIVATE void checkConflicts()
{
    /*
     * Checks pending conflicts. If any then launch
     * cdar process
     */
        int ifIndex;
        NAME *cName;
        CONFLICT *current;

        if (RunningCDAR)
                return;
        pthread_mutex_lock(&ConflictMutex);
        current = getNextConflict(Conflicts);
        if (current == NULL) {
                pthread_mutex_unlock(&ConflictMutex);
                return;
        }
        cName = current->cName;
        ifIndex = current->ifIndex;
        if (Conflicts->logPath != NULL)
                logConflict(current,Conflicts->logPath);
        remConflict(current,Conflicts);
        RunningCDAR = 1;
        invokeCdar(_CDARCONFLICT,ifIndex,cName);
        pthread_mutex_unlock(&ConflictMutex);
}

PRIVATE int checkName(char *name, int ifIndex, U_CHAR *T)
{
    /*
     * Checks the name queried name against the 'NAME' list.
     * In other words: The query was for me?
     * Note: Header is reused. Mark 'T' bit allready
     */
        int i;
        NAME *current;

        if (name == NULL)
                return FAILURE;
        current = Names;
        for (; current != NULL; current = current->next) {
                if (current->name == NULL)
                        continue;
                if (strncasecmp(current->name,name,strlen(current->name)))
                        continue;
                for (i = 0; i < current->notAuthOnSz; i++) {
                        if (current->notAuthOn[i] == ifIndex)
                                return FAILURE;
                }
                *T = current->nameStatus;
                return SUCCESS;
        }
        return FAILURE;
}

PRIVATE int checkPtrName(char *name, int ifIndex, int family)
{
    /*
     * Same thing that checkName() but this time with 'PTR' names
     */
        int ret;
        NETIFACE *iface;

        ret = FAILURE;
        iface = getNetIfNodeByIndex(Ifaces,ifIndex);
        if (iface == NULL)
                return FAILURE;
        if (family == AF_INET) {
                ret = _checkPtrName(name,iface->IPv4s);
                if (ret)
                        ret = __checkPtrName(name,iface->IPv6s);
        } else if (family == AF_INET6) {
                ret = __checkPtrName(name,iface->IPv6s);
                if (ret)
                        ret = _checkPtrName(name,iface->IPv4s);
        }
        return ret;
}

PRIVATE int _checkPtrName(char *name, NETIFIPV4 *ipv4s)
{
    /*
     * checkPtrName() helper for "IPv4 names"
     */
        NETIFIPV4 *current;

        if (ipv4s == NULL)
                return FAILURE;
        current = ipv4s;
        for (; current != NULL; current = current->next) {
                if (current->ipType == NOIP)
                        continue;
                if (!strcasecmp(current->PTR4RECORD,name))
                        return SUCCESS;
        }
        return FAILURE;
}

PRIVATE int __checkPtrName(char *name, NETIFIPV6 *ipv6s)
{
    /*
     * checkPtrName() helper for "IPv6 names"
     */
        NETIFIPV6 *current;

        if (ipv6s == NULL)
                return FAILURE;
        current = ipv6s;
        for (; current != NULL; current = current->next) {
                if (current->ipType == NOIP)
                        continue;
                if (!strcasecmp(current->PTR6RECORD,name))
                        return SUCCESS;
        }
        return FAILURE;
}

PRIVATE void setDescriptorToPoll(POLLFD *pollArr, int i, int fd)
{
    /*
     * Given a socket, add it to the array of sockets to poll()
     * Note: position 0 of the array is reserved for the UDP (IPv4)
     * main socket. Position 1 to the UDP (IPv6) main socket. 2 for
     * TCP (IPv4 & IPv6) main socket and position 4 for netlink
     * socket
     */
        if (fd <= 0) {
                handleError(ESOCKERR,NULL,i);
                pollArr[i].fd = -1;
                pollArr[i].events = 0;

                if (pollArr[0].fd < 0 && pollArr[1].fd < 0 &&
                    pollArr[2].fd < 0) {
                        logError(FORCED_EXIT,NULL);
                        freeResources(pollArr);
                }
                return;
        }
        pollArr[i].fd = fd;
        pollArr[i].events = POLLIN;
}

PRIVATE void removeDescriptorFromPoll(POLLFD *pollArr, int i)
{
    /*
     * Remove a socket from the poll() array
     */
        if (pollArr[i].fd > 0) {
                close(pollArr[i].fd);
                pollArr[i].fd = -1;
                pollArr[i].events = 0;
        }
}

PRIVATE void sigTermHandler(int signo)
{
    /*
     * when 'SIGTERM' received infinite loop ends
     * (Stop daemon)
     */
        signo = signo;
        Flag = 0;
        return;
}

PRIVATE void sigUsr2Handler(int signo)
{
    /*
     * When 'SIGUSR2' received print a lot of things
     * (Debug purposes)
     */
        signo = signo;
        printNames(Names);
        printIfaces(Ifaces);
        //printRList(Rlist);
        return;
}

PRIVATE void handleError(int err, POLLFD *pollArr, int i)
{
    /*
     * Handle and log errors
     */
        int newFd;
        switch (err) {
        case EPOLLERR:
                removeDescriptorFromPoll(pollArr,i);
                switch (i) {
                case 0:
                        newFd = createUdpSocket(AF_INET);
                        setDescriptorToPoll(pollArr,0,newFd);
                        break;
                case 1:
                        newFd = createUdpSocket(AF_INET6);
                        setDescriptorToPoll(pollArr,1,newFd);
                        break;
                case 2:
                        newFd = createTcpSock();
                        setDescriptorToPoll(pollArr,2,newFd);
                        break;
                case 3:
                        newFd = createNetLinkSocket();
                        setDescriptorToPoll(pollArr,3,newFd);
                        break;
                }
                break;
        case ESOCKERR:
                switch (i) {
                case 0:
                        logError(ESOCKERR,"IPv4 UDP socket");
                        break;
                case 1: logError(ESOCKERR,"IPv6 UDP socket");
                        break;
                case 2:
                        logError(ESOCKERR,"TCP socket");
                        break;
                case 3:
                        logError(ESOCKERR,"Netlink socket");
                        break;
                }
                break;
        }
}

PRIVATE void freeResources(POLLFD *pollArr)
{
        closeLog();
        //closeStream();
        removeDescriptorFromPoll(pollArr,0);
        removeDescriptorFromPoll(pollArr,1);
        removeDescriptorFromPoll(pollArr,2);
        removeDescriptorFromPoll(pollArr,3);
        if (Names != NULL)
                deleteNameList(&Names);
        if (Ifaces != NULL)
                delNetIfList(&Ifaces);
        if (Rlist != NULL)
                deleteRList(&Rlist);
        if (Conflicts != NULL)
                deleteConflictList(&Conflicts);
        exit(EXIT_SUCCESS);
}
