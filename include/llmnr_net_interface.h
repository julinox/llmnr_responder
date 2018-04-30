/** ******************************************************
 * Interface to handle network interfaces                *
 * It stores the associated ip address, interface index, *
 * name and also a variable flag which describe a set    *
 * of states regarding a network interface               *
 *********************************************************/

#ifndef LLMNR_NET_INTERFACE_H
#define LLMNR_NET_INTERFACE_H

#define ARECORDSZ 10 + 4
#define AAAARECORDSZ 10 + 16
#define PTR4RECORDSZ 30
#define PTR6RECORDSZ 75
#define AF_DUAL AF_INET + AF_INET6

enum IF_FLAGS {
    /*
     * - _STATIC means the interface were added by config file
     *   and not dinamically by the system
     * - _INET and _INET6 means the interface has an IPv4
     *   or IPV6
     * - _STATICINET and STATICINET6 means the interface
     *   is forced to operate with IPv4 or IPv6 (specified by
     *   the config file)
     * - _CONFLICT means the interface has a mirror interface
     * - _CDAR means the interface allready did cdar (see
     *   llmnr_conflict.h)
     */
        _IFF_NOIF = 1, /* List head */
        _IFF_STATIC = 1 << 1,
        _IFF_DYNAMIC = 1 << 2,
        _IFF_LOOPBACK = 1 << 3,
        _IFF_RUNNING = 1 << 4,
        _IFF_INET = 1 << 5,
        _IFF_INET6 = 1 << 6,
        _IFF_MCAST4 = 1 << 7,
        _IFF_MCAST6 = 1 << 8,
        _IFF_STATICINET = 1 << 9,
        _IFF_STATICINET6 = 1 << 10,
        _IFF_CONFLICT = 1 << 11,
        _IFF_CDAR = 1 << 12
};

enum _IPTYPE {
        IPV4IP,
        IPV6IP,
        GLOBALIP,
        PUBLICIP,
        PRIVATEIP,
        LINKLOCALIP,
        MULTICASTIP,
        UNIQUELOCALIP,
        NOIP /* List head */
};

/*
 * Struct that holds an IPv4 associated to an interface. Also
 * holds their corresponding 'A' record. 'PTR4RECORD' will hold
 * the "in-addr.arpa" name associated with an IPv4
 */
typedef struct netIPv4 {
        U_CHAR ipType;
        struct in_addr ip4Addr;
        U_CHAR ARECORD[ARECORDSZ];
        char PTR4RECORD[PTR4RECORDSZ];
        struct netIPv4 *next;
} NETIFIPV4;

/*
 * Same thing as 'NETIFIPV4' but this time with IPv6
 */
typedef struct netIPv6 {
        U_CHAR ipType;
        struct in6_addr ip6Addr;
        U_CHAR AAAARECORD[AAAARECORDSZ];
        char PTR6RECORD[PTR6RECORDSZ];
        struct netIPv6 *next;
} NETIFIPV6;

/*
 * Generic network interface struct
 * 'mirroIfs' arrays hold the mirror interfaces regarding
 * this interface
 * Mirror interface: Other local network Interface that
 * operates in the same network
 */
typedef struct netIface {
        char *name;
        int flags;
        int ifIndex;
        U_SHORT mirrorIfSz;
        U_CHAR mirrorIfs[MAXIFACES];
        NETIFIPV4 *IPv4s;
        NETIFIPV6 *IPv6s;
        struct netIface *next;
} NETIFACE;

//PUBLIC NETIFACE *newNetIfList();
PUBLIC NETIFACE *NetIfListHead();
PUBLIC NETIFACE *getNetIfNodeByIndex(NETIFACE *ifaces, int ifIndex);
PUBLIC NETIFACE *getNetIfNodeByName(NETIFACE *ifaces, char *name);
PUBLIC NETIFIPV4 *getNetIfIpv4Node(NETIFACE *iface, INADDR *ip4);
PUBLIC NETIFIPV6 *getNetIfIpv6Node(NETIFACE *iface, IN6ADDR *ip6);
PUBLIC void delNetIfList(NETIFACE **ifaces);
PUBLIC void remNetIfNode(char *name, NETIFACE *ifaces);
PUBLIC void remNetIfIPv4(NETIFACE *iface, INADDR *remove);
PUBLIC void remNetIfIPv6(NETIFACE *iface, IN6ADDR *remove);
PUBLIC void addMirrorIf(NETIFACE *iface, int ifIndex);
PUBLIC void delMirrorIf(NETIFACE *iface, int ifIndex);
PUBLIC void printIfaces(NETIFACE *ifaces);
PUBLIC int addNetIfNode(char *name, int family, int flags, NETIFACE *ifaces);
PUBLIC int addNetIfIPv4(INADDR *ip, NETIFACE *iface);
PUBLIC int addNetIfIPv6(IN6ADDR *ip, NETIFACE *iface);
PUBLIC int ip6Type(IN6ADDR *ip);
PUBLIC int netIfaceListSz(NETIFACE *ifaces);
PUBLIC INADDR *getFirstValidAddr(NETIFACE *iface);

#endif
