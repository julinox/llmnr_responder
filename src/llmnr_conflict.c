/* Macros */
#define CHECK 1
#define _GNU_SOURCE
#define POLLSOCKSZ 2
#define QUESTMINSZ 17
#define QUERYMAXTRIES 3
//#define PRIORITY_IPV4

/* Includes */
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <netinet/in.h>

/* Own includes */
#include "../include/llmnr_defs.h"
#include "../include/llmnr_names.h"
#include "../include/llmnr_net_interface.h"
#include "../include/llmnr_rr.h"
#include "../include/llmnr_utils.h"
#include "../include/llmnr_sockets.h"
#include "../include/llmnr_packet.h"
#include "../include/llmnr_conflict.h"

/* Enums & Structs */
enum RESFLAGS {
        _NONE = 1 << 0,
        _SELF = 1 << 1,
        _WON = 1 << 2,
        _LOST = 1 << 3
};

typedef struct {
        U_SHORT id;
        NAME *name;
        U_CHAR *jitter;
        NETIFACE *cIface;
        NETIFACE *ifaces;
} RCVMSG;

/* Prototypes */
PRIVATE void sendQuery(int fd4, int fd6, U_CHAR *pktBuffer, int pktSz);
PRIVATE void wonAction(RCVMSG *params);
PRIVATE void lostAction(RCVMSG *params);
PRIVATE int recvMsg(int fd4, int fd6, RCVMSG *params);
PRIVATE int recvMsg4(int fd, RCVMSG *params);
PRIVATE int recvMsg6(int fd, RCVMSG *params);
PRIVATE int _recvMsg4(RCVMSG *params, struct msghdr *msg);
PRIVATE int _recvMsg6(RCVMSG *params, struct msghdr *msg);
PRIVATE int createPkt(char *name, U_SHORT id, U_CHAR *pktBuffer);
PRIVATE int lexCmp(void *ownIp, void *peerIp, int ipSz);

/* Glocal variables */

/* Functions definitions */
PUBLIC void cDar(NAME *name, NETIFACE *iface, NETIFACE *ifs)
{
    /*
     * - "Creates" a LLMNR query packet, quering 'name' for 'ANY'
     *   resource record. The packet id is randomly created
     * - "Creates" two sending sockets, IPv4 and IPv6
     * - "Sends" two packets (IPv4 and IPv6), forcing 'iface'
     *   as exit interface
     * - "Checks" if any response was received
     * - If no response received then send again (See RFC 4795)
     * - If after every attempt no response was received then
     *   "mark me" as winner
     * - Once its done marks 'name' as checked
     * - Note: doble quotes words are delegated actions
     * - Note 2: struct RCVMSG just holds a list of function parameters
     */
        RCVMSG params;
        U_CHAR *pktBuffer;
        NETIFACE *currentIf;
        int count, fd4, fd6, pktSz;

        pktBuffer = calloc(1,SNDBUFSZ);
        if (pktBuffer == NULL)
                return;
        if (name == NULL || name->name == NULL)
                return;
        if (ifs == NULL)
                return;
        if (iface == NULL)
                return;
        if (iface->flags & _IFF_NOIF)
                return;

        fd4 = 0;
        fd6 = 0;
        count = 0;
        pktSz = 0;
        currentIf = iface;
        memset(&params,0,sizeof(RCVMSG));
        params.id = (U_SHORT)random();
        params.name = name;
        params.cIface = iface;
        params.ifaces = ifs;
        params.jitter = NULL;
        pktSz = createPkt(name->name,params.id,pktBuffer);
        fd4 = createC4Sock(getFirstValidAddr(currentIf));
        fd6 = createC6Sock(currentIf->ifIndex);

        if (fd4 < 0 && fd6 < 0)
                return;
        while (count < QUERYMAXTRIES) {

                sendQuery(fd4,fd6,pktBuffer,pktSz);
                if (!recvMsg(fd4,fd6,&params))
                        break;
                count++;
        }
        if (count >= QUERYMAXTRIES)
                wonAction(&params);
        if (fd4 > 0)
                close(fd4);
        if (fd6 > 0)
                close(fd6);
        name->nameStatus = OWNER;
        free(pktBuffer);
}

PRIVATE int recvMsg(int fd4, int fd6, RCVMSG *params)
{
    /*
     * Wait 'LLMNR_TIMEOUT' for potential responses (See RFC 4795)
     * According to a defined priority, first "receive" a pontential
     * response on one socket (either IPv4 or IPv6). If no response
     * then check the other socket. A priority is used because on
     * potenital responses a winner must be determined. The winner is
     * selected by comparing the lower ip address between the
     */
        poll(0,0,LLMNR_TIMEOUT);
        #ifdef PRIORITY_IPV4
                if (!recvMsg4(fd4,params))
                        return SUCCESS;
                else if (!recvMsg6(fd6,params))
                        return SUCCESS;
        #else
                if (!recvMsg6(fd6,params))
                        return SUCCESS;
                else if (!recvMsg4(fd4,params))
                        return SUCCESS;
        #endif
        return FAILURE;
}

PRIVATE int recvMsg4(int fd, RCVMSG *params)
{
    /*
     * - Receive all potential IPv4 responses (may be none or
     *   several responses)
     * - recvmsg instead of recvfrom (needs to know destiny
     *   ip addres in the response)
     * - ancBuffer will hold the ancillary data (See socket api)
     * - Once a response its received "checks" if the response
     *   came from one of my own interfaces. In that case the two
     *   involved interfaces are marked as 'mirror interfaces'
     * - If the response came from other peer then "checks" who
     *   wins the conflict (See RFC 4795)
     * - a flag variable (resFlag) is used beacause there might
     *   be several responses, and i need to keep track of the
     *   outcome of every response to make a final decision about
     *   the 'overall' winner in this conflict resolution
     * - Note: for winning a conflict a peer must prevail (or win)
     *   against every response received
     * - Note: doble quote works are delegated actions
     */
        int rcved;
        SA_IN from;
        struct iovec iov;
        struct msghdr msg;
        U_CHAR resFlag, jitter;
        U_CHAR rcvBuffer[RCVBUFSZ];
        U_CHAR ancBuffer[ANCBUFSZ];

        if (fd < 0)
                return FAILURE;
        jitter = 0;
        memset(&iov,0,sizeof(iov));
        memset(&msg,0,sizeof(msg));
        iov.iov_base = rcvBuffer;
        iov.iov_len = RCVBUFSZ;
        msg.msg_name = &from;
        msg.msg_namelen = sizeof(from);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = ancBuffer;
        msg.msg_controllen = ANCBUFSZ;
        resFlag = _NONE;
        params->jitter = &jitter;

        if (params->ifaces == NULL)
                return SUCCESS;
        if (!(params->cIface->flags & _IFF_RUNNING))
                return SUCCESS;
        while (CHECK) {
                memset(&from,0,sizeof(from));
                memset(rcvBuffer,0,RCVBUFSZ);
                memset(ancBuffer,0,ANCBUFSZ);
                rcved = recvmsg(fd,&msg,MSG_DONTWAIT);
                if (rcved < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                                break;
                }
                if (rcved < QUESTMINSZ)
                        continue;
                resFlag |= _recvMsg4(params,&msg);
        }
        if (resFlag & _LOST)
                lostAction(params);
        else if (resFlag & _WON )
                wonAction(params);
        else if (resFlag & _SELF || resFlag & _NONE)
                return FAILURE;
        return SUCCESS;
}

PRIVATE int recvMsg6(int fd, RCVMSG *params)
{
    /*
     * See 'recvMsg4()'. Same thing but with IPv6
     */
        int rcved;
        SA_IN6 from;
        struct iovec iov;
        struct msghdr msg;
        U_CHAR resFlag, jitter;
        U_CHAR rcvBuffer[RCVBUFSZ];
        U_CHAR ancBuffer[ANCBUFSZ];


        if (fd < 0)
                return FAILURE;
        jitter = 0;
        memset(&iov,0,sizeof(iov));
        memset(&msg,0,sizeof(msg));
        iov.iov_base = rcvBuffer;
        iov.iov_len = RCVBUFSZ;
        msg.msg_name = &from;
        msg.msg_namelen = sizeof(from);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = ancBuffer;
        msg.msg_controllen = ANCBUFSZ;
        resFlag = _NONE;
        params->jitter = &jitter;

        if (params->ifaces == NULL)
                return SUCCESS;
        if (!(params->cIface->flags & _IFF_RUNNING))
                return SUCCESS;
        while (CHECK) {
                memset(&from,0,sizeof(from));
                memset(rcvBuffer,0,RCVBUFSZ);
                memset(ancBuffer,0,ANCBUFSZ);
                rcved = recvmsg(fd,&msg,MSG_DONTWAIT);
                if (rcved < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                                break;
                }
                if (rcved < QUESTMINSZ)
                        continue;
                resFlag |= _recvMsg6(params,&msg);
        }
        if (resFlag & _LOST)
                lostAction(params);
        else if (resFlag & _WON )
                wonAction(params);
        else if (resFlag & _SELF || resFlag & _NONE)
                return FAILURE;
        return SUCCESS;
}

PRIVATE int _recvMsg4(RCVMSG *params, struct msghdr *msg)
{
    /*
     * On a given IPv4 respond checks several things:
     * - Check if Answer id packet matches the query
     * - Check if Answer name matches the query
     * - Check if Answer came from the same interface for which
     *   the query was sent
     * - Check if Answer came from one of my own interfaces (if
     *   so then mark both interfaces as 'mirror interfaces')
     * - "Checks" the conflict resolution winner
     */
        HEADER head;
        void *rcvBuffer;
        NETIFIPV4 *ipv4s;
        NETIFACE *current;
        INADDR toIp ,*fromIp;

        current = params->ifaces;
        memset(&head,0,sizeof(head));
        rcvBuffer = msg->msg_iov->iov_base;
        getHeader(rcvBuffer,&head);
        if (params->id != head.ID)
                return _NONE;
        if (strncasecmp(params->name->name,(char *)(rcvBuffer + HEADSZ),
                        strlen(params->name->name)))
                return _NONE;
        if (head.C && !*params->jitter) {
                poll(0,0,JITTER_INTERVAL);
                *params->jitter = 0xA;
        }
        __getPktInfo(AF_INET,&toIp,msg);
        fromIp = &(((SA_IN *)msg->msg_name)->sin_addr);

        for (; current != NULL; current = current->next) {
                if (current->flags & _IFF_NOIF)
                        continue;
                if (!(current->flags & _IFF_RUNNING))
                        continue;
                if (current->ifIndex == params->cIface->ifIndex)
                        continue;
                ipv4s = current->IPv4s;
                for (; ipv4s != NULL; ipv4s = ipv4s->next) {
                        if (ipv4s->ipType == NOIP)
                                continue;
                        if (!memcmp(fromIp,&ipv4s->ip4Addr,sizeof(INADDR))) {
                                addMirrorIf(params->cIface,current->ifIndex);
                                addMirrorIf(current,params->cIface->ifIndex);
                                params->cIface->flags |= _IFF_CONFLICT;
                                current->flags |= _IFF_CONFLICT;
                                return _SELF;
                        }
                }
        }
        return lexCmp(&toIp,fromIp,sizeof(INADDR));
}

PRIVATE int _recvMsg6(RCVMSG *params, struct msghdr *msg)
{
    /*
     * See _recvMsg4(). Same thing but with IPv6
     */
        HEADER head;
        void *rcvBuffer;
        NETIFIPV6 *ipv6s;
        NETIFACE *current;
        IN6ADDR toIp ,*fromIp;

        current = params->ifaces;
        memset(&head,0,sizeof(head));
        rcvBuffer = msg->msg_iov->iov_base;
        getHeader(rcvBuffer,&head);
        if (params->id != head.ID)
                return _NONE;
        if (strncasecmp(params->name->name,(char *)(rcvBuffer + HEADSZ),
                        strlen(params->name->name)))
                return _NONE;
        if (head.C && !*params->jitter) {
                poll(0,0,JITTER_INTERVAL);
                *params->jitter = 0xB;
        }
        __getPktInfo(AF_INET6,&toIp,msg);
        fromIp = &(((SA_IN6 *)msg->msg_name)->sin6_addr);
        for (; current != NULL; current = current->next) {
                if (current->flags & _IFF_NOIF)
                        continue;
                if (current->ifIndex == params->cIface->ifIndex)
                        continue;
                if (!(current->flags & _IFF_RUNNING))
                        continue;
                ipv6s = current->IPv6s;
                for (; ipv6s != NULL; ipv6s = ipv6s->next) {
                        if (ipv6s->ipType == NOIP)
                                continue;
                        if (!memcmp(fromIp,&ipv6s->ip6Addr,sizeof(IN6ADDR))) {
                                addMirrorIf(params->cIface,current->ifIndex);
                                addMirrorIf(current,params->cIface->ifIndex);
                                params->cIface->flags |= _IFF_CONFLICT;
                                current->flags |= _IFF_CONFLICT;
                                return _SELF;
                        }
                }
        }
	if (head.T == 0)
		return _LOST;
        return lexCmp(&toIp,fromIp,sizeof(IN6ADDR));
}

PRIVATE int createPkt(char *name, U_SHORT id, U_CHAR *pktBuffer)
{
    /*
     * "Creates" a LLMNR query packet asking for 'name'
     * with 'ANY' resource record
     */
        int pktSz;
        HEADER head;
        QUERY query;

        pktSz = 0;
        memset(&head,0,sizeof(HEADER));
        memset(&query,0,sizeof(QUERY));
        head.ID = id;
        head.QDCOUNT = 1;
        query.QNAME = name;
        query.QTYPE = ANY;
        query.QCLASS = INCLASS;
        pktSz += attachHeader(&head,pktBuffer);
        pktSz += attachQuery(&query,pktBuffer + HEADSZ);
        return pktSz;
}

PRIVATE void sendQuery(int fd4, int fd6, U_CHAR *pktBuffer, int pktSz)
{
    /*
     * Send a packet to a multicast destiny (224.0.0.252 and FF::1:3)
     */
        SA_IN dest4;
        SA_IN6 dest6;

        fillMcastDest(&dest4,&dest6);
	#ifdef PRIORITY_IPV4
        	if (fd4 > 0)
            		sendto(fd4,pktBuffer,pktSz,0,(SA *)&dest4,sizeof(SA_IN));
        	if (fd6 > 0)
            		sendto(fd6,pktBuffer,pktSz,0,(SA *)&dest6,sizeof(SA_IN6));
	#else
		if (fd6 > 0)
			sendto(fd6,pktBuffer,pktSz,0,(SA *)&dest6,sizeof(SA_IN6));
		if (fd4 > 0)
			sendto(fd4,pktBuffer,pktSz,0,(SA *)&dest4,sizeof(SA_IN));
	#endif
}

PRIVATE int lexCmp(void *ownIp, void *peerIp, int ipSz)
{
    /*
     * Lexicography comparation between two ips
     */
        if (memcmp(ownIp,peerIp,ipSz) > 0)
                return _LOST;
        return _WON;
}

PRIVATE void wonAction(RCVMSG *params)
{
    /*
     * - Add to 'NAME' authoritative list the 'current' interface
     * - Checks if any mirror interface belongs to 'NAME'
     *   no-authoritativelist. If so then remove it and add it to
     *   'NAME' authoritative list
     * - NOTE: RCVMSG holds a list of parameters
     */

        int i;
        NAME *name;
        NETIFACE *iface;

        name = params->name;
        iface = params->cIface;
        addAuthOn(name,iface->ifIndex);
        for (i = 0; i < iface->mirrorIfSz; i++) {
                if (!isNotAuthOn(name,iface->mirrorIfs[i])) {
                    delNotAuthOn(name,iface->mirrorIfs[i]);
                    addAuthOn(name,iface->mirrorIfs[i]);
                }
        }
        iface->flags |= _IFF_CDAR;
}

PRIVATE void lostAction(RCVMSG *params)
{
    /*
     * - Checks if any mirror interface belongs to 'NAME'
     *   authoritative list. If so then the conflict wast'n
     *   lost and 'current' interface is added to 'NAME'
     *   authoritative list
     * - If no mirror interface won the conflict then add
     *   'current' interface to 'NAME' no-authoritative list
     */
        int i, j;
        NAME *name;
        U_CHAR res;
        NETIFACE *iface;

        res = _LOST;
        name = params->name;
        iface = params->cIface;
        for (i = 0; i < name->authOnSz; i++) {
                for (j = 0; j < iface->mirrorIfSz; j++) {
                        if (iface->mirrorIfs[j] == name->authOn[i])
                                res = _WON;
                }
        }
        if (res == _LOST)
                addNotAuthOn(name,iface->ifIndex);
        if (res == _WON)
                addAuthOn(name,iface->ifIndex);
        iface->flags |= _IFF_CDAR;
}

