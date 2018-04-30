/* Macros */

/* Includes */
#include <stdlib.h>
#include <string.h>
#include <net/if.h>
#include <netinet/in.h>

/* Own Includes */
#include "../include/llmnr_defs.h"
#include "../include/llmnr_print.h"
#include "../include/llmnr_net_interface.h"

/* Enums & Structs */

/* Private prototypes */
PRIVATE void printType(NETIFIPV6 *ip6);
PRIVATE void printFlags(NETIFACE *iface);
PRIVATE void delNetIfIPv4List(NETIFIPV4 **ipv4s);
PRIVATE void delNetIfIPv6List(NETIFIPV6 **ipv6s);
PRIVATE void updateIfaceStatus(NETIFACE *iface);
PRIVATE int countIps(void *ipList, int iptype);
PRIVATE int repeatedInterface(int family, NETIFACE *iface);
PRIVATE NETIFIPV4 *newNetIfIPv4List();
PRIVATE NETIFIPV6 *newNetIfIPv6List();

/* Glocal variables */

/* Functions definitions */
PUBLIC NETIFACE *NetIfListHead()
{
    /*
     * NETIFACE list head
     */
        NETIFACE *head;

        head = calloc(1,sizeof(NETIFACE));
        if (head == NULL)
                return NULL;
        head->name = NULL;
        head->ifIndex = -255;
        head->flags = _IFF_NOIF;
        head->IPv4s = NULL;
        head->IPv6s = NULL;
        head->next = NULL;
        return head;
}

PUBLIC int addNetIfNode(char *name, int family, int flags, NETIFACE *ifaces)
{
    /*
     * - Add a new 'NETIFACE' node to the list
     * - This is an ordered list (ordered by 'ifIndex')
     * - Before creating a new node first checks if a node with 'name'
     *   allready exists. In that case 'repeatedInterface' is called
     * Note: a repeated inteface can happens when interfaces are added
     * by config file and not dinamically
     */
        int ifIndex;
        NETIFACE *current, *previous, *newNode;

        if (ifaces == NULL)
                return SUCCESS;
        if (name == NULL)
                return SUCCESS;
        current = ifaces;
        previous = current;
        ifIndex = if_nametoindex(name);

        for (; current != NULL; current = current->next) {
                if (current->name == NULL || current->ifIndex < 0)
                        continue;
                if (!strcmp(name,current->name))
                        return repeatedInterface(family,current);
                if (ifIndex > current->ifIndex) {
                        if (current->ifIndex == 0)
                                break;
                        previous = current;
                        continue;
                }
                if (ifIndex == 0) {
                        previous = current;
                        continue;
                }
                break;
        }
        newNode = calloc(1,sizeof(NETIFACE));
        if (newNode == NULL)
                return SYSFAILURE;
        newNode->name = calloc(1,strlen(name) + 1);
        if (newNode->name == NULL)
                return SYSFAILURE;
        strcpy(newNode->name,name);
        newNode->ifIndex = ifIndex;
        newNode->flags |= flags;
        newNode->IPv4s = NULL;
        newNode->IPv6s = NULL;
        newNode->next = previous->next;
        previous->next = newNode;
        if (newNode->flags & _IFF_STATIC)
                repeatedInterface(family,newNode);
        return SUCCESS;
}

PUBLIC int addNetIfIPv4(INADDR *ip, NETIFACE *iface)
{
    /*
     * Add a new 'NETIFIPV4' to a given interface
     * Once an interface acquires (for the first time) an ip
     * addres (IPv4 in this case) its considered as a running interface
     * Note: when an interface is '_STATIC' if flag '_STATICINET' is not
     * set then the interface can't operate with IPv4
     * Note 2: Remeber that network interfaces goes up and down dinamically
     */
        NETIFIPV4 *current, *last, *newNode;

        if (iface == NULL)
                return SUCCESS;
        if (iface->IPv4s == NULL)
                iface->IPv4s = newNetIfIPv4List();
        if (iface->IPv4s == NULL)
                return SUCCESS;
        if (iface->flags & _IFF_STATIC) {
                if (!(iface->flags & _IFF_STATICINET))
                        return SUCCESS;
        }

        current = iface->IPv4s;
        while(current != NULL) {
                if (!memcmp(&current->ip4Addr,ip,sizeof(INADDR)))
                        return FAILURE;
                last = current;
                current = current->next;
        }
        newNode = calloc(1,sizeof(NETIFIPV4));
        if (newNode == NULL)
                return SUCCESS;
        memcpy(&newNode->ip4Addr,ip,sizeof(INADDR));
        newNode->next = NULL;
        iface->flags |= _IFF_INET;
        iface->flags |= _IFF_RUNNING;
        newNode->ipType = IPV4IP;
        last->next = newNode;
        return SUCCESS;
}

PUBLIC int addNetIfIPv6(IN6ADDR *ip, NETIFACE *iface)
{
    /*
     * Same explanation that has 'addNetIfIPv4' but for IPv6
     */
        NETIFIPV6 *current, *last, *newNode;

        if (iface == NULL)
                return SUCCESS;
        if (iface->flags & _IFF_STATIC) {
                if (!(iface->flags & _IFF_STATICINET6))
                        return SUCCESS;
        }
        if (iface->IPv6s == NULL)
                iface->IPv6s = newNetIfIPv6List();
        if (iface->IPv6s == NULL)
                return SUCCESS;
        current = iface->IPv6s;
        while (current != NULL) {
                if (!memcmp(&current->ip6Addr,ip,sizeof(IN6ADDR)))
                        return SUCCESS;
                last = current;
                current = current->next;
        }
        newNode = calloc(1,sizeof(NETIFIPV6));
        if (newNode == NULL)
                return SUCCESS;
        memcpy(&newNode->ip6Addr,ip,sizeof(IN6ADDR));
        newNode->next = NULL;
        iface->flags |= _IFF_INET6;
        iface->flags |= _IFF_RUNNING;
        newNode->ipType = ip6Type(ip);
        last->next = newNode;
        return SUCCESS;
}

PUBLIC void delNetIfList(NETIFACE **ifaces)
{
    /*
     * Delete a 'NETIFACE' list
     */
        NETIFACE *current, *dispose;

        if (*ifaces == NULL)
                return;
        current = *ifaces;
        while (current != NULL) {
                dispose = current;
                current = current->next;
                if (dispose->name != NULL)
                        free(dispose->name);
                delNetIfIPv4List(&dispose->IPv4s);
                delNetIfIPv6List(&dispose->IPv6s);
                free(dispose);
        }
        *ifaces = NULL;
}

PUBLIC void remNetIfNode(char *name, NETIFACE *ifaces)
{
    /*
     * Remove a 'NETIFACE' node from list
     */
        NETIFACE *current, *previous, *dispose;

        if (ifaces == NULL)
                return;
        if (name == NULL)
                return;
        current = ifaces;
        previous = current;
        for (; current != NULL; current = current->next) {
                if (current->name == NULL)
                        continue;
                if (!(strncasecmp(name,current->name,strlen(name)))) {
                        dispose = current;
                        previous->next = current->next;
                        free(dispose->name);
                        delNetIfIPv4List(&dispose->IPv4s);
                        delNetIfIPv6List(&dispose->IPv6s);
                        free(dispose);
                        return;
                }
                previous = current;
        }
}

PUBLIC void remNetIfIPv4(NETIFACE *iface, INADDR *remove)
{
    /*
     * remove a 'NETIFIPV4' node from a given interface
     */
        NETIFIPV4 *current, *previous, *dispose;

        if (iface == NULL)
                return;
        current = iface->IPv4s;
        previous = current;
        for (; current != NULL; current = current->next) {
                if (current->ipType == NOIP)
                        continue;
                if (!memcmp(&current->ip4Addr,remove,sizeof(INADDR))) {
                        dispose = current;
                        previous->next = current->next;
                        free(dispose);
                        updateIfaceStatus(iface);
                        break;
                }
                previous = current;
        }
}

PUBLIC void remNetIfIPv6(NETIFACE *iface, IN6ADDR *remove)
{
    /*
     * remove a 'NETIFIPV6' node from a given interface
     */
        NETIFIPV6 *current, *previous, *dispose;

        if (iface == NULL)
                return;
        current = iface->IPv6s;
        previous = current;
        for (; current != NULL; current = current->next) {
                if (current->ipType == NOIP)
                        continue;
                if (!memcmp(&current->ip6Addr,remove,sizeof(IN6ADDR))) {
                        dispose = current;
                        previous->next = current->next;
                        free(dispose);
                        updateIfaceStatus(iface);
                        break;
                }
                previous = current;
        }
}

PUBLIC NETIFACE *getNetIfNodeByIndex(NETIFACE *ifaces, int ifIndex)
{
    /*
     * Given a 'NETIFACE' node, returns his interface index
     */
        NETIFACE *current;

        if (ifaces == NULL)
                return NULL;
        current = ifaces;
        while (current != NULL) {
                if (current->ifIndex == ifIndex)
                        return current;
                current = current->next;
        }
        return NULL;
}

PUBLIC NETIFACE *getNetIfNodeByName(NETIFACE *ifaces, char *name)
{
    /*
     * Given a 'NETIFACE' node, returns his interface name
     */
        NETIFACE *current;

        if (ifaces == NULL)
                return NULL;
        if (name == NULL)
                return NULL;
        current = ifaces;
        for (; current != NULL; current = current->next) {
                if (current->name == NULL)
                        continue;
                if (!strcasecmp(current->name,name))
                        return current;
        }
        return NULL;
}

PUBLIC NETIFIPV4 *getNetIfIpv4Node(NETIFACE *iface, INADDR *ip4)
{
    /*
     * Given an IPv4 returns a 'NETIFACE' node that holds
     * the given ip address
     */
        NETIFIPV4 *current;

        if (iface == NULL)
                return NULL;
        if (ip4 == NULL)
                return NULL;
        if (iface->IPv4s == NULL)
                return NULL;
        current = iface->IPv4s;
        for (; current != NULL; current = current->next) {
                if (!memcmp(&current->ip4Addr,ip4,sizeof(INADDR)))
                        return current;
        }
        return NULL;
}

PUBLIC NETIFIPV6 *getNetIfIpv6Node(NETIFACE *iface, IN6ADDR *ip6)
{
    /*
     * Given an IPv6 returns a 'NETIFACE' node that holds
     * the given ip address
     */
        NETIFIPV6 *current;

        if (iface == NULL)
                return NULL;
        if (ip6 == NULL)
                return NULL;
        if (iface->IPv6s == NULL)
                return NULL;
        current = iface->IPv6s;
        for (; current != NULL; current = current->next) {
                if (!memcmp(&current->ip6Addr,ip6,sizeof(IN6ADDR)))
                        return current;
        }
        return NULL;
}

PUBLIC INADDR *getFirstValidAddr(NETIFACE *iface)
{
    /*
     * Given a 'NETIFACE' node, returns their first valid IPv4
     */
        NETIFIPV4 *current;

        if (iface == NULL)
                return NULL;
        if (iface->IPv4s == NULL)
                return NULL;
        current = iface->IPv4s;
        for (; current != NULL; current = current->next) {
                if (current->ipType == IPV4IP)
                        return &current->ip4Addr;
        }
        return NULL;
}

PUBLIC int netIfaceListSz(NETIFACE *ifaces)
{
    /*
     * Count the size of a 'NETIFACE' list
     */
        int count;
        NETIFACE *current;

        count = 0;
        if (ifaces == NULL)
                return count;
        current = ifaces;
        for (; current != NULL; current = current->next)
                count++;
        return count;
}

PUBLIC int ip6Type(IN6ADDR *ip)
{
    /*
     * Given an IPv6 returns his kind
     */
        int type;

        type = IPV6IP;
        if (ip == NULL)
                return FAILURE;
        if (ip->s6_addr[0] == 0xFE && (ip->s6_addr[1] >> 6) == 2)
                type = LINKLOCALIP;
        else if (ip->s6_addr[0] == 0xFC || ip->s6_addr[0] == 0xFD)
                type = UNIQUELOCALIP;
        else if (ip->s6_addr[0] == 0xFF)
                type = MULTICASTIP;
        return type;
}

PUBLIC void addMirrorIf(NETIFACE *iface, int ifIndex)
{
    /*
     * Add a mirror interface index to the 'mirroIfs' array
     */
        int i;

        for (i = 0; i < iface->mirrorIfSz; i++) {
                if (iface->mirrorIfs[i] == ifIndex)
                        return;
        }
        iface->mirrorIfs[iface->mirrorIfSz++] = ifIndex;
}

PUBLIC void delMirrorIf(NETIFACE *iface, int ifIndex)
{
    /*
     * remove an interface index from the 'mirrorIfs' array
     */
        U_CHAR *ptr;
        int i, mark;

        mark = -1;
        ptr = iface->mirrorIfs;
        for (i = 0; i < iface->mirrorIfSz; i++) {
                if (mark >= 0) {
                        ptr[mark++] = ptr[i];
                        ptr[i] = 0;
                }
                if (ptr[i] == ifIndex) {
                        mark = i;
                        if (iface->mirrorIfSz == 1) {
                                ptr[i] = 0;
                                break;
                        }
                }
        }
        if (mark >= 0)
                iface->mirrorIfSz--;
}

PRIVATE int repeatedInterface(int family, NETIFACE *iface)
{
    /*
     * Update the flags status regarding a '_STATIC' interface
     * This flags (_STATIC, _STATICINETand _STATICINET6) exist
     * to avoid reading again a config file when an interface goes
     * down or up. So if an '_STATIC' interface goes down and then
     * later in time goes up again, we need to know what protocols
     * (IPv4 or Ipv6) can the interface operate
     */
        if (iface->flags & _IFF_DYNAMIC)
                return SUCCESS;
        if (family == AF_INET) {
                iface->flags |= _IFF_STATICINET;
        } else if (family == AF_INET6) {
                iface->flags |= _IFF_STATICINET6;
        } else if (family == AF_DUAL) {
                iface->flags |= _IFF_STATICINET;
                iface->flags |= _IFF_STATICINET6;
        }
        return SUCCESS;
}

PRIVATE void updateIfaceStatus(NETIFACE *iface)
{
    /*
     * Update (if necessary) interface status when an ip is erased
     */
        if (countIps(iface->IPv4s,IPV4IP) <= 1) {
                iface->flags &= ~_IFF_INET;
        }
        if (countIps(iface->IPv6s,IPV6IP) <= 1)
                iface->flags &= ~_IFF_INET6;
        if (!(iface->flags & _IFF_INET) && !(iface->flags & _IFF_INET6)) {
                iface->flags &= ~_IFF_RUNNING;
                iface->flags &= ~_IFF_CONFLICT;
                iface->flags &= ~_IFF_CDAR;
        }

}

PRIVATE int countIps(void *ipList, int iptype)
{
    /*
     * given an ip list (might be 'NETIFIPV4' or 'NETIFIPV6') count
     * how many ips has
     */
        int i;
        NETIFIPV4 *ip4;
        NETIFIPV6 *ip6;

        i = 0;
        if (iptype == IPV4IP) {
                ip4 = (NETIFIPV4 *)ipList;
                for (; ip4 != NULL; ip4 = ip4->next)
                        i++;
        } else if (iptype == IPV6IP) {
                ip6 = (NETIFIPV6 *)ipList;
                for (; ip6 != NULL; ip6 = ip6->next)
                        i++;
        }
        return i;
}

PRIVATE NETIFIPV4 *newNetIfIPv4List()
{
    /*
     * 'NETIFIPV4' list head
     */
        NETIFIPV4 *head;

        head = calloc(1,sizeof(NETIFIPV4));
        if (head == NULL)
                return NULL;
        head->ipType = NOIP;
        head->next = NULL;
        return head;
}

PRIVATE NETIFIPV6 *newNetIfIPv6List()
{
    /*
     * 'NETIFIPV6' list head
     */
        NETIFIPV6 *head;

        head = calloc(1,sizeof(NETIFIPV6));
        if (head == NULL)
                return NULL;
        head->ipType = NOIP;
        head->next = NULL;
        return head;
}

PRIVATE void delNetIfIPv4List(NETIFIPV4 **ipv4s)
{
    /*
     * Delete a 'NETIFIPV4' list
     */
        NETIFIPV4 *current, *dispose;

        if (*ipv4s == NULL)
                return;
        current = *ipv4s;
        while (current != NULL) {
                dispose = current;
                current = current->next;
                free(dispose);
        }
        *ipv4s = NULL;
}

PRIVATE void delNetIfIPv6List(NETIFIPV6 **ipv6s)
{
    /*
     * Delete a 'NETIFIPV6' list
     */
        NETIFIPV6 *current, *dispose;

        if (*ipv6s == NULL)
                return;
        current = *ipv6s;
        while (current != NULL) {
                dispose = current;
                current = current->next;
                free(dispose);
        }
        ipv6s = NULL;
}

PUBLIC void printIfaces(NETIFACE *ifaces)
{
        int i;
        NETIFACE *aux;
        NETIFIPV4 *ip4;
        NETIFIPV6 *ip6;

        if (ifaces == NULL)
                return;
        aux = ifaces;
        for (; aux != NULL; aux = aux->next) {
                printToStream("Name: %s. Index: %d\n",
                       aux->name,aux->ifIndex);
                printToStream("mirroSz %d:  ",aux->mirrorIfSz);
                for (i = 0; i < aux->mirrorIfSz; i++)
                        printToStream("%d, ",aux->mirrorIfs[i]);
                printToStream("\n");
                printFlags(aux);
                ip4 = aux->IPv4s;
                while (ip4 != NULL) {
                        if (ip4->ipType == IPV4IP)
                                printToStream("Type: IPv4\n");
                        else if (ip4->ipType == NOIP)
                                printToStream("Type: List head\n");
                        else
                                printToStream("Unk IPv4 type\n");
                        printBytes(&ip4->ip4Addr,sizeof(INADDR));
                        printBytes(ip4->ARECORD,sizeof(ip4->ARECORD));
                        nPrintBytes(ip4->PTR4RECORD,sizeof(ip4->PTR4RECORD));
                        ip4 = ip4->next;
                }
                ip6 = aux->IPv6s;
                while (ip6 != NULL) {
                        printType(ip6);
                        printBytes(&ip6->ip6Addr,sizeof(IN6ADDR));
                        printBytes(&ip6->AAAARECORD,sizeof(ip6->AAAARECORD));
                        nPrintBytes(ip6->PTR6RECORD,sizeof(ip6->PTR6RECORD));
                        ip6 = ip6->next;
                }
                printToStream("\n--------------------------------------------------\n");
        }
}

PRIVATE void printType(NETIFIPV6 *ip6)
{
        printToStream("Type: ");
        switch (ip6->ipType) {
        case IPV6IP:
                printToStream("IPv6\n");
                break;
        case LINKLOCALIP:
                printToStream("Link Local\n");
                break;
        case MULTICASTIP:
                printToStream("Multicast\n");
                break;
        case UNIQUELOCALIP:
                printToStream("Unique local\n");
                break;
        case NOIP:
                printToStream("List head\n");
                break;
        default:
                printToStream("%d\n",ip6->ipType);
        }
}

PRIVATE void printFlags(NETIFACE *iface)
{
        int fl;

        fl = iface->flags;
        printToStream("noif: %s. static: %s. dynamic: %s. loopback: %s\n",
               fl&_IFF_NOIF ? "yes":"no",fl&_IFF_STATIC ? "yes" : "no",
               fl&_IFF_DYNAMIC ? "yes" : "no",fl&_IFF_LOOPBACK ? "yes" : "no");
        printToStream("running: %s. inet: %s. inet6: %s. mcast4: %s\n",
               fl&_IFF_RUNNING ? "yes" : "no", fl&_IFF_INET ? "yes" : "no",
               fl&_IFF_INET6 ? "yes" : "no",fl&_IFF_MCAST4 ? "yes" : "no");
        printToStream("mcast6. %s. stainet: %s. stainet6: %s. conflict: %s. cdar: %s\n",
               fl&_IFF_MCAST6 ? "yes" : "no", fl&_IFF_STATICINET ? "yes" : "no",
               fl&_IFF_STATICINET6 ? "yes" : "no", fl&_IFF_CONFLICT ? "yes" : "no",
               fl&_IFF_CDAR ? "yes" : "no");
}
