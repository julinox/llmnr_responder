/* Macros */
#define BACKLOG 5
#define _GNU_SOURCE

/* Includes */
#include <poll.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <linux/rtnetlink.h>

/* Own includes */
#include "../include/llmnr_defs.h"
#include "../include/llmnr_utils.h"
#include "../include/llmnr_net_interface.h"
#include "../include/llmnr_sockets.h"

/* Enums & Structs */

/* Private prototypes */
PRIVATE int addMembership(int fd, int fam, int ifIndex, INADDR *addr);
PRIVATE int dropMembership(int fd, int fam, int ifIndex, INADDR *addr);

/* Glocal variables */
PRIVATE INADDR MCAST4;
PRIVATE IN6ADDR MCAST6;

/* Functions definitions */
PUBLIC int createNetLinkSocket()
{
    /*
     * Creates a Netlink socket and sucribe to 'RTMGRP_IPV4_IFADDR'
     * and 'RTMGRP_IPV6_IFADDR' "multicast" groups. Through this im
     * allowed to know changes on network interfaces
     */
        int newFd;
        struct sockaddr_nl bindNl;

        newFd = 0;
        memset(&bindNl,0,sizeof(bindNl));
        bindNl.nl_family = AF_NETLINK;
        bindNl.nl_pid = getpid();
        bindNl.nl_groups = RTMGRP_IPV4_IFADDR |
                           RTMGRP_IPV6_IFADDR;
        /* RTMGRP_LINK Future reference (maybe) */

        newFd = socket(AF_NETLINK,SOCK_RAW,NETLINK_ROUTE);
        if (newFd < 0)
                return FAILURE;
        if (bind(newFd,(SA *)&bindNl,sizeof(bindNl)) < 0) {
                return FAILURE;
        }
        return newFd;
}

PUBLIC int createUdpSocket(int family)
{
    /*
     * Creates main udp socket for listening petitions (IPv4 & IPv6)
     * Bind it to 'LLMNRPORT' and set several socket options like:
     * - IP_PKTINFO
     * - IPV6_RECVPKTINFO
     * - IPV6_V6ONLY
     * - IP_TTL
     * - IPV6_UNICAST_HOPS
     */
        SA_IN bind4;
        SA_IN6 bind6;
        int fd, yes, res;

        fd = 0;
        if (family == AF_INET) {
                memset(&bind4,0,sizeof(bind4));
                fd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
                if (fd < 0)
                        return FAILURE;
                yes = 1;
                res = setsockopt(fd,IPPROTO_IP,IP_PKTINFO,&yes,sizeof(yes));
                if (res < 0)
                        return FAILURE;
                yes = 0xFF;
                setsockopt(fd,IPPROTO_IP,IP_TTL,&yes,sizeof(yes));
                bind4.sin_family = AF_INET;
                bind4.sin_port = htons(LLMNRPORT);
                bind4.sin_addr.s_addr = INADDR_ANY;
                res = bind(fd,(SA *)&bind4,sizeof(bind4));
                if (res < 0)
                        return FAILURE;
        }

        else if (family == AF_INET6) {
                memset(&bind6,0,sizeof(bind6));
                fd = socket(AF_INET6,SOCK_DGRAM,IPPROTO_UDP);
                if (fd < 0)
                        return FAILURE;
                yes = 1;
                res = setsockopt(fd,IPPROTO_IPV6,IPV6_RECVPKTINFO,&yes,
                                 sizeof(yes));
                if (res < 0)
                        return FAILURE;
                setsockopt(fd,IPPROTO_IPV6,IPV6_V6ONLY,&yes,sizeof(yes));
                yes = 0xFF;
                setsockopt(fd,IPPROTO_IPV6,IPV6_UNICAST_HOPS,&yes,sizeof(yes));
                bind6.sin6_family = AF_INET6;
                bind6.sin6_port = htons(LLMNRPORT);
                bind6.sin6_addr = in6addr_any;
                res = bind(fd,(SA *)&bind6,sizeof(bind6));
                if (res < 0)
                        return FAILURE;
        }
        return fd;
}

PUBLIC int createTcpSock()
{
    /*
     * Creates listening TCP scoket. RFC 4795 stays that
     * a responder must listen on both protocols (UDP and TCP)
     * This creates one dual-stack socket
     */
        SA_IN6 bind6;
        int newFd, no, yes;

        no = 0;
        yes = 1;
        newFd = 0;
        memset(&bind6,0,sizeof(SA_IN6));

        bind6.sin6_family = AF_INET6;
        bind6.sin6_port = htons(LLMNRPORT);
        bind6.sin6_addr = in6addr_any;

        newFd = socket(AF_INET6,SOCK_STREAM,IPPROTO_TCP);
        if (newFd < 0)
                return FAILURE;
        if (setsockopt(newFd,IPPROTO_IPV6,IPV6_V6ONLY,&no,sizeof(no)) < 0) {
                close(newFd);
                return FAILURE;
        }
        setsockopt(newFd,IPPROTO_IP,IP_TTL,&yes,sizeof(int));
        setsockopt(newFd,IPPROTO_IPV6,IPV6_UNICAST_HOPS,&yes,sizeof(yes));
        if (bind(newFd, (SA *)&bind6,sizeof(SA_IN6)) < 0) {
                close(newFd);
                return FAILURE;
        }
        listen(newFd,BACKLOG);
        return newFd;
}

PUBLIC int createC4Sock(INADDR *ip)
{
    /*
     * Creates an IPv4 socket for sending multicast (to 224.0.0.252)
     * packets. This sockets is used when resolving conflicts
     * IP_MUlTICAST_LOOP disabled.
     */
        int fd;
        SA_IN bindd;
        INADDR ipv4;
        unsigned int on;

        if (ip == NULL)
                return FAILURE;
        fd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        if (fd < 0)
                return FAILURE;
        on = 0;
        memcpy(&ipv4,ip,sizeof(INADDR));
        setsockopt(fd,IPPROTO_IP,IP_MULTICAST_LOOP,(void *)&on,
                       sizeof(unsigned int));
        on = 1;
        setsockopt(fd,IPPROTO_IP,IP_MULTICAST_TTL,(void *)&on,
                       sizeof(unsigned int));
        if (setsockopt(fd,IPPROTO_IP,IP_PKTINFO,(void *)&on,
                       sizeof(unsigned int))) {
                close(fd);
                return FAILURE;
        }

        memset(&bindd,0,sizeof(bindd));
        bindd.sin_family = AF_INET;
        bindd.sin_port = 0;
        if (bind(fd,(SA *)&bindd,sizeof(bindd))) {
                close(fd);
                return FAILURE;
        }
        if (setsockopt(fd,IPPROTO_IP,IP_MULTICAST_IF,(void *)&ipv4,
                       sizeof(INADDR))) {
                close(fd);
                return FAILURE;
        }
        return fd;
}

PUBLIC int createC6Sock(int ifIndex)
{
    /*
     * Same thing that createC4Sock() but with IPv6
     */
        int fd;
        SA_IN6 bindd;
        unsigned int on;

        fd = socket(AF_INET6,SOCK_DGRAM,IPPROTO_UDP);
        if (fd < 0)
                return FAILURE;
        on = 0;
        setsockopt(fd,IPPROTO_IPV6,IPV6_MULTICAST_LOOP,(void *)&on,
                   sizeof(unsigned int));
        on = 1;
        setsockopt(fd,IPPROTO_IPV6,IPV6_MULTICAST_HOPS,(void *)&on,
                   sizeof(int));
        setsockopt(fd,IPPROTO_IPV6,IPV6_V6ONLY,(void *)&on,
                   sizeof(unsigned int));
        if (setsockopt(fd,IPPROTO_IPV6,IPV6_RECVPKTINFO,(void *)&on,
                       sizeof(unsigned int)))
                return FAILURE;
        memset(&bindd,0,sizeof(bindd));
        bindd.sin6_family = AF_INET6;
        bindd.sin6_port = 0;
        if (bind(fd,(SA *)&bindd,sizeof(bindd))) {
                close(fd);
                return FAILURE;
        }
        if (setsockopt(fd,IPPROTO_IPV6,IPV6_MULTICAST_IF,(void *)&ifIndex,
                       sizeof(unsigned int))) {
                close(fd);
                return FAILURE;
        }
        return fd;
}

PUBLIC void joinMcastGroup(POLLFD *polling, NETIFACE *iface)
{
    /*
     * Given an interface ('NETIFACE') this function will join
     * that interface to the LLMNR multicast groups (224.0.0.252
     * and FF02::1:3)
     * Note: polling[0].fd holds the IPv4 UDP main socket
     * Note2: polling[1].fd holds the IPv6 UDP main socket
     */
        INADDR *ip4;

        ip4 = NULL;
        if (!(iface->flags & _IFF_RUNNING))
                return;
        if (iface->flags & _IFF_INET) {
                if (!(iface->flags & _IFF_MCAST4)) {
                        ip4 = getFirstValidAddr(iface);
                        if (!addMembership(polling[0].fd,AF_INET,0,ip4))
                                iface->flags |= _IFF_MCAST4;
                }
        }
        if (iface->flags & _IFF_INET6) {
                if (!(iface->flags & _IFF_MCAST6)) {
                        if (!addMembership(polling[1].fd,AF_INET6,
                                     iface->ifIndex,NULL))
                                iface->flags |= _IFF_MCAST6;
                }
        }
}

PUBLIC void leaveMcastGroup(POLLFD *polling, NETIFACE *iface, INADDR *copy)
{
    /*
     * Same thing that joinMcastGroup() but this time with leaving
     */
        if (!(iface->flags & _IFF_INET)) {
                if (iface->flags & _IFF_MCAST4) {
                        if (!dropMembership(polling[0].fd,AF_INET,0,copy))
                                iface->flags &= ~_IFF_MCAST4;

                }
        }
        if (!(iface->flags & _IFF_INET6)) {
                if (iface->flags & _IFF_MCAST6) {
                        if (!dropMembership(polling[1].fd,AF_INET6,
                                           iface->ifIndex,NULL))
                                iface->flags &= ~_IFF_MCAST6;
                }
        }
}

PRIVATE int addMembership(int fd, int fam, int ifIndex, INADDR *addr)
{
    /*
     * Do the acutal join of the interface. For IPv4 its required
     * the ip addres of the interface that wants to join. For IPv6
     * is enough with the interface index.
     */
        int res;
        struct ip_mreq mreq;
        struct ipv6_mreq mreq6;

        res = 0;
        if (fd <= 0)
                return SUCCESS;
        if (fam == AF_INET && addr != NULL) {
                memset(&mreq,0,sizeof(mreq));
                memcpy(&mreq.imr_multiaddr,&MCAST4,sizeof(INADDR));
                memcpy(&mreq.imr_interface,addr,sizeof(INADDR));
                res = setsockopt(fd,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,
                           sizeof(mreq));
        }

        else if (fam == AF_INET6 && ifIndex > 0) {
                memset(&mreq6,0,sizeof(mreq6));
                memcpy(&mreq6.ipv6mr_multiaddr,&MCAST6,sizeof(IN6ADDR));
                mreq6.ipv6mr_interface = ifIndex;
                res = setsockopt(fd,IPPROTO_IPV6,IPV6_ADD_MEMBERSHIP,&mreq6,
                           sizeof(mreq6));
        }
        if (res < 0)
                return FAILURE;
        return SUCCESS;
}

PRIVATE int dropMembership(int fd, int fam, int ifIndex, INADDR *addr)
{
    /*
     * Same thing that addMembership() but this is to drop
     * a previuos join
     */
        int res;
        struct ip_mreq mreq;
        struct ipv6_mreq mreq6;

        res = 0;
        if (fd <= 0)
                return SUCCESS;
        if (fam == AF_INET && addr != NULL) {
                memset(&mreq,0,sizeof(mreq));
                memcpy(&mreq.imr_multiaddr,&MCAST4,sizeof(INADDR));
                memcpy(&mreq.imr_interface,addr,sizeof(INADDR));
                res = setsockopt(fd,IPPROTO_IP,IP_DROP_MEMBERSHIP,&mreq,
                                 sizeof(mreq));
        } else if (fam == AF_INET6 && ifIndex > 0) {
                memset(&mreq6,0,sizeof(mreq6));
                memcpy(&mreq6.ipv6mr_multiaddr,&MCAST6,sizeof(IN6ADDR));
                mreq6.ipv6mr_interface = ifIndex;
                res = setsockopt(fd,IPPROTO_IPV6,IPV6_DROP_MEMBERSHIP,
                                 &mreq6,sizeof(mreq6));
        }
        if (res < 0)
                return FAILURE;
        return SUCCESS;
}

PUBLIC int getPktInfo(int family, struct msghdr *msg, UDPCLIENT *client)
{
    /*
     * Get some extra info about a received packet. This extra info
     * includes:
     * - Know the interface number from where it came the packet
     * - Know the destiny ip address of the packet (If a query
     *   is sent to a no multicast address it must be discarded)
     * - Ancillary data is used (see recvmsg() socket api)
     */
        SA_IN6 *ip6;
        struct cmsghdr *cmsgPtr;

        cmsgPtr = CMSG_FIRSTHDR(msg);
        for (; cmsgPtr != NULL; cmsgPtr = CMSG_NXTHDR(msg,cmsgPtr)) {
                if (family == AF_INET && cmsgPtr->cmsg_level == IPPROTO_IP
                    && cmsgPtr->cmsg_type == IP_PKTINFO) {
                        client->pktinfo4 = (struct in_pktinfo *) CMSG_DATA(cmsgPtr);
                        client->recviface = client->pktinfo4->ipi_ifindex;
                        client->iptype = IPV4IP;
                        return isMulticast(&client->pktinfo4->ipi_addr,
                                              &MCAST4,AF_INET);

                } else if (family == AF_INET6
                           && cmsgPtr->cmsg_level == IPPROTO_IPV6
                           && cmsgPtr->cmsg_type == IPV6_PKTINFO) {
                        client->pktinfo6 = (struct in6_pktinfo*) CMSG_DATA(cmsgPtr);
                        client->recviface = client->pktinfo6->ipi6_ifindex;
                        ip6 = (SA_IN6 *)&client->from;
                        client->iptype = ip6Type(&ip6->sin6_addr);
                        return isMulticast(&client->pktinfo6->ipi6_addr,&MCAST6,
                                           AF_INET6);
                }
        }
        return FAILURE;
}

PUBLIC int __getPktInfo(int family, void *ip, struct msghdr *msg)
{
    /*
     * Same thing that getPktInfo() but this is a little bit
     * different. This returns the destiny ip address and the interface
     * from where it came the packet (in getPktInfo() the destiny ip is not
     * returned). This function is used for responses in conflict detection
     * (see llmnr_conflict.h)
     */
        int ifIndex;
        struct cmsghdr *cmsgPtr;
        struct in_pktinfo *pktInfo4;
        struct in6_pktinfo *pktInfo6;

        ifIndex = 0;
        cmsgPtr = CMSG_FIRSTHDR(msg);
        for (; cmsgPtr != NULL; cmsgPtr = CMSG_NXTHDR(msg,cmsgPtr)) {
                if (family == AF_INET && cmsgPtr->cmsg_level == IPPROTO_IP
                    && cmsgPtr->cmsg_type == IP_PKTINFO) {
                        pktInfo4 = (struct in_pktinfo *) CMSG_DATA(cmsgPtr);
                        ifIndex = pktInfo4->ipi_ifindex;
                        memcpy(ip,&pktInfo4->ipi_addr,sizeof(INADDR));

                } else if (family == AF_INET6
                           && cmsgPtr->cmsg_level == IPPROTO_IPV6
                           && cmsgPtr->cmsg_type == IPV6_PKTINFO) {
                        pktInfo6 = (struct in6_pktinfo*) CMSG_DATA(cmsgPtr);
                        ifIndex = pktInfo6->ipi6_ifindex;
                        memcpy(ip,&pktInfo6->ipi6_addr,sizeof(IN6ADDR));
                }
        }
        return ifIndex;
}

PUBLIC void getTcpPktInfo(SA_IN6 *name, TCPCLIENT *client, NETIFACE *ifaces)
{
    /*
     * Same thing that getPktInfo(). Ancillary data isn't available
     * in TCP sockets. Instead used getsockname() to know the destiny
     * ip address. With the destiny ip we obtain the interface that
     * received the packet (Looking a match through the 'NETIFACE'list)
     * Note: The TCP socket is a dual-stack socket (Works with IPv6 & IPv4)
     */
        INADDR *ip4;
        IN6ADDR *ip6;
        NETIFACE *current;

        current = ifaces;
        if (name->sin6_addr.s6_addr[0] == 0 &&
            name->sin6_addr.s6_addr[10] == 0xFF &&
            name->sin6_addr.s6_addr[11] == 0xFF) {

                ip4 = (INADDR *) ((name->sin6_addr.s6_addr) + 12);
                for (; current != NULL; current = current->next) {
                        if (current->flags & _IFF_NOIF)
                                continue;
                        if (getNetIfIpv4Node(current,ip4) != NULL) {
                                client->recvIface = current->ifIndex;
                                client->ipType = IPV4IP;
                                break;
                        }
                }
        } else {
                ip6 = &name->sin6_addr;
                for (; current != NULL; current = current->next) {
                        if (current->flags & _IFF_NOIF)
                                continue;
                        if (getNetIfIpv6Node(current,ip6) != NULL) {
                                client->recvIface = current->ifIndex;
                                client->ipType = ip6Type(ip6);
                        }
                }
        }
}

PUBLIC void fillMcastVars()
{
    /*
     * fill the structs (in_addr and in6_addr) that will
     * hold the LLMNR multicast address (224.0.0.252 and
     * FF02::1:3)
     */
        memset(&MCAST4,0,sizeof(MCAST4));
        memset(&MCAST6,0,sizeof(MCAST6));
        fillMcastIp(&MCAST4,&MCAST6);
}

/*PUBLIC int createC4Sock()
{
    // IPv4
        int fd;
        SA_IN bindd;
        unsigned int on;

        fd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        if (fd < 0)
                return FAILURE;
        on = 0;
        setsockopt(fd,IPPROTO_IP,IP_MULTICAST_LOOP,(void *)&on,
                       sizeof(unsigned int));
        on = 1;
        setsockopt(fd,IPPROTO_IP,IP_MULTICAST_TTL,(void *)&on,
                       sizeof(unsigned int));
        if (setsockopt(fd,IPPROTO_IP,IP_PKTINFO,(void *)&on,
                       sizeof(unsigned int)))
                return FAILURE;

        memset(&bindd,0,sizeof(bindd));
        bindd.sin_family = AF_INET;
        bindd.sin_port = 0;
        if (bind(fd,(SA *)&bindd,sizeof(bindd)))
                return FAILURE;
        return fd;
}*/

/*PUBLIC int createC6Sock()
{
    //IPv6
        int fd;
        SA_IN6 bindd;
        unsigned int on;

        fd = socket(AF_INET6,SOCK_DGRAM,IPPROTO_UDP);
        if (fd < 0)
                return FAILURE;
        on = 0;
        setsockopt(fd,IPPROTO_IPV6,IPV6_MULTICAST_LOOP,(void *)&on,
                   sizeof(unsigned int));
        on = 1;
        setsockopt(fd,IPPROTO_IPV6,IPV6_MULTICAST_HOPS,(void *)&on,
                   sizeof(int));
        setsockopt(fd,IPPROTO_IPV6,IPV6_V6ONLY,(void *)&on,
                   sizeof(unsigned int));
        if (setsockopt(fd,IPPROTO_IPV6,IPV6_RECVPKTINFO,(void *)&on,
                       sizeof(unsigned int)))
                return FAILURE;
        memset(&bindd,0,sizeof(bindd));
        bindd.sin6_family = AF_INET6;
        bindd.sin6_port = 0;
        if (bind(fd,(SA *)&bindd,sizeof(bindd)))
                return FAILURE;
        return fd;
}*/
