/* Macros */
#define _GNU_SOURCE
#define HEADERFIELDS 6
#define HEADERFLAGS 10

/* Includes */
#include <string.h>
#include <netinet/in.h>

/* Own includes */
#include "../include/llmnr_defs.h"
#include "../include/llmnr_names.h"
#include "../include/llmnr_net_interface.h"
#include "../include/llmnr_rr.h"
#include "../include/llmnr_packet.h"

/* Enums & Structs */
typedef union {
        struct cmsghdr hdr;
        U_CHAR buff[CMSG_SPACE(sizeof(struct in_pktinfo))];
} IPV4CMSG;

/* Private functions prototypes */
PRIVATE int _attachAnswer(PKTPARAMS *paras, DSTRUCTURE *dsts, int offset);
PRIVATE int attachARecord(PKTPARAMS *params, DSTRUCTURE *dsts, int offset);
PRIVATE int attachAAAARecord(PKTPARAMS *params, DSTRUCTURE *dsts, int offset);
PRIVATE int _attachAAARecord(PKTPARAMS *params, DSTRUCTURE *dsts, int offset);
PRIVATE int __attachAAARecord(PKTPARAMS *params, DSTRUCTURE *dsts, int offset);
PRIVATE int attachOtherecord(PKTPARAMS *params, DSTRUCTURE *dsts, int offset);
PRIVATE int attachAnyRecord(PKTPARAMS *params, DSTRUCTURE *dsts, int offset);
PRIVATE int attachPtrRecord(PKTPARAMS *params, DSTRUCTURE *dsts, int offset);
PRIVATE int _sendPacket(PKTSND *pktSnd);
PRIVATE int testEndianness();
PRIVATE void copyShortValues(U_CHAR *dest, U_CHAR *values, int valuesSz);
PRIVATE U_SHORT flagsToShortValue(U_CHAR *flags);

/* Glocal variables */

/* Functions definitions */
PUBLIC void getHeader(U_CHAR *buffer, HEADER *head)
{
    /*
     * Given a buffer, extract the LLMNR header within
     * and store it into a 'HEADER' struct
     */
        U_CHAR byte1, byte2;

        if (head == NULL)
                return;
        byte1 = *((U_CHAR *)buffer + 2);
        byte2 = *((U_CHAR *)buffer + 3);
        head->ID = ntohs(*((U_SHORT *)buffer));
        head->QR = ((0x1 << 7) & byte1) >> 7;
        head->OPCODE = ((0xF << 3) & byte1) >> 3;
        head->C = ((0x1 << 2) & byte1) >> 2;
        head->TC = ((0x1 << 1) & byte1) >> 1;
        head->T = 0x1 & byte1;
        head->Z0 = ((0x1 << 7) & byte2) >> 7;
        head->Z1 = ((0x1 << 6) & byte2) >> 6;
        head->Z2 = ((0x1 << 5) & byte2) >> 5;
        head->Z3 = ((0x1 << 4) & byte2) >> 4;
        head->RCODE = 0xF & byte2;
        head->QDCOUNT = ntohs(*((U_SHORT *)(buffer + 4)));
        head->ANCOUNT = ntohs(*((U_SHORT *)(buffer + 6)));
        head->NSCOUNT = ntohs(*((U_SHORT *)(buffer + 8)));
        head->ARCOUNT = ntohs(*((U_SHORT *)(buffer + 10)));
}

PUBLIC void getQuery(U_CHAR *buffer, QUERY *query)
{
    /*
     * Given a buffer, extract the LLMNR query within
     * and store it into a 'QUERY' struct
     */
        query->QNAME = (char *)buffer + HEADSZ;
        query->QTYPE = *((U_SHORT *)(buffer + HEADSZ +
                          strlen(query->QNAME) + 1));
        query->QTYPE = ntohs(query->QTYPE);
        query->QCLASS = *((U_SHORT *)(buffer + HEADSZ +
                           strlen(query->QNAME) + 1 + sizeof(U_SHORT)));
        query->QCLASS = ntohs(query->QCLASS);
}

PUBLIC int attachHeader(HEADER *header, U_CHAR *pktBuff)
{
    /*
     * Given a 'HEADER' struct, "decompose" it and store it into
     * 'pktBuff'. Returns the number of bytes copied into 'pktBuff'
     * in this case will always be 'HEADSZ'
     */
        U_CHAR headerFlags[HEADERFLAGS];
        U_SHORT headerValues[HEADERFIELDS];

        memset(headerFlags,0,HEADERFLAGS);
        memset(headerValues,0,HEADERFIELDS);
        headerFlags[0] = header->QR ;
        headerFlags[1] = header->OPCODE;
        headerFlags[2] = header->C;
        headerFlags[3] = header->TC;
        headerFlags[4] = header->T;
        headerFlags[5] = header->Z0;
        headerFlags[6] = header->Z1;
        headerFlags[7] = header->Z2;
        headerFlags[8] = header->Z3;
        headerFlags[9] = header->RCODE;
        headerValues[0] = header->ID;
        headerValues[1] = flagsToShortValue(headerFlags);
        headerValues[2] = header->QDCOUNT;
        headerValues[3] = header->ANCOUNT;
        headerValues[4] = header->NSCOUNT;
        headerValues[5] = header->ARCOUNT;
        copyShortValues(pktBuff,(U_CHAR *)headerValues,HEADERFIELDS);
        return HEADSZ;
}

PUBLIC int attachQuery(QUERY *query, U_CHAR *pktBuff)
{
    /*
     * Given a 'QUERY' struct, "decompose" it and store it into
     * 'pktBuff'. Returns the numbers of bytes copied into 'pktBuff'
     * in this case: strlen of QNAME + QTYPE (2 bytes) + QCLASS (2 bytes)
     * Returns the number of bytes wrote into the buffer
     */
        int pktSz;
        U_SHORT aux;
        int u_shortSz;

        pktSz = 0;
        u_shortSz = sizeof(U_SHORT);
        strcpy((char *)pktBuff,query->QNAME);
        pktSz = strlen(query->QNAME) + 1;
        aux = htons(query->QTYPE);
        memcpy(pktBuff + pktSz,&aux,u_shortSz);
        pktSz += u_shortSz;
        aux = htons(query->QCLASS);
        memcpy(pktBuff + pktSz,&aux,u_shortSz);
        pktSz += u_shortSz;
        return pktSz;
}

PUBLIC int attachAnswer(PKTPARAMS *params, DSTRUCTURE *dsts)
{
    /*
     * Construct a whole LLMNR answer: HEADER + QUERY + ANSWER
     * 'HEADER' is last attached because some of the 'HEADER'
     * values are relative to the content of 'QUERY' and 'ANSWER'
     * Note: The resource records that the daemon holds are stored
     * into 3 different places, 'A' and 'AAAA' in a 'NETIFACE'
     * list. 'PTR' in a 'NAME' list. Any other than that ('SOA',
     * 'MX', etc) in a 'RRLIST'.
     * Returns the number of bytes wrote into the buffer
     */
        int pktSz;

        pktSz = attachQuery(params->query,params->pktBuff + HEADSZ);
        pktSz += _attachAnswer(params,dsts,pktSz + HEADSZ);
        pktSz += attachHeader(params->head,params->pktBuff);
        return pktSz;
}

PRIVATE int _attachAnswer(PKTPARAMS *params, DSTRUCTURE *dsts, int offset)
{
    /*
     * Helper function for attachAnswer(). Checks the type of 'ANSWER'
     * required and then choose the proper function to fetch the
     * desired resource record (s). Also, if no resource record found
     * then attach the 'SOA' record (empty answer)
     * Returns the number of bytes wrote into the buffer
     */
        int pktSz;
        U_SHORT type;
        NETIFACE *iface;

        pktSz = 0;
        type = params->query->QTYPE;
        params->head->ANCOUNT = 0;
        if (type == A)
                pktSz = attachARecord(params,dsts,offset);
        else if (type == AAAA)
                pktSz = attachAAAARecord(params,dsts,offset);
        else if (type == ANY)
                pktSz = attachAnyRecord(params,dsts,offset);
        else if (type == PTR)
                pktSz = attachPtrRecord(params,dsts,offset);
        else
                pktSz = attachOtherecord(params,dsts,offset);
        params->head->QR = 1;
        iface = getNetIfNodeByIndex(dsts->ifaces,params->rcvIface);
        if (iface != NULL) {
                if (iface->flags & _IFF_CONFLICT)
                        params->head->C = 1;
        }
        if (params->head->ANCOUNT == 0) {
                params->query->QTYPE = SOA;
                pktSz = attachOtherecord(params,dsts,offset);
                params->head->ANCOUNT = 0;
                params->head->NSCOUNT = 1;
        }
        return pktSz;
}

PRIVATE int attachARecord(PKTPARAMS *params, DSTRUCTURE *dsts, int offset)
{
    /*
     * Fetch and write into a buffer all 'A' records relative to an
     * interface (the one that received the query)
     * Note: Remember that LLMNR operates on a local network, so it
     * makes no sense fetching 'A' records from a network interface
     * operating in some other network link
     * Returns the number of bytes wrote into the buffer
     */
        U_SHORT uSz;
        U_CHAR *pktBuff;
        NETIFACE *iface;
        NETIFIPV4 *current;
        int pktSz, anCount;

        pktSz = 0;
        anCount = 0;
        uSz = sizeof(U_SHORT);
        pktBuff = params->pktBuff + offset;
        iface = getNetIfNodeByIndex(dsts->ifaces,params->rcvIface);
        current = iface->IPv4s;
        if (current == NULL) {
                params->head->ANCOUNT = anCount;
                return pktSz;
        }

        for (; current != NULL; current = current->next) {
                if (current->ipType == NOIP)
                        continue;
                if (offset + pktSz + uSz + ARECORDSZ > params->pktBuffSz) {
                        params->head->TC = 1;
                        break;
                }
                memcpy(pktBuff,params->namePtr,uSz);
                pktBuff += uSz;
                memcpy(pktBuff,current->ARECORD,ARECORDSZ);
                anCount++;
                pktBuff += ARECORDSZ;
                pktSz += uSz + ARECORDSZ;
        }
        params->head->ANCOUNT += anCount;
        return pktSz;
}

PRIVATE int attachAAAARecord(PKTPARAMS *params, DSTRUCTURE *dsts, int offset)
{
    /*
     * Same thing that attachARecord() except for IPv6 this time.
     * Nevertheless there is something important to remark:
     * If the query came from IPv4 (layer 3) then i can answer back
     * with all relative 'AAAA' records in no particular order.
     * However if the query came from IPv6 (layer 3) then i should
     * answer back with some order: first the 'AAAA' records that have
     * the same IPv6 kind that have the query, then all remaining
     * 'AAAA' records
     */
        int pktSz;

        pktSz = 0;
        if (params->ipType == IPV4IP)
                pktSz = _attachAAARecord(params,dsts,offset);
        else
                pktSz = __attachAAARecord(params,dsts,offset);
        return pktSz;
}

PRIVATE int _attachAAARecord(PKTPARAMS *params, DSTRUCTURE *dsts, int offset)
{
        /*
         * Query came from IPv4, so order does not matter
         * Returns the number of bytes wrote into the buffer
         */

        U_SHORT uSz;
        U_CHAR *pktBuff;
        NETIFACE *iface;
        NETIFIPV6 *current;
        int pktSz, anCount;

        pktSz = 0;
        anCount = 0;
        uSz = sizeof(U_SHORT);
        pktBuff = params->pktBuff + offset;
        iface = getNetIfNodeByIndex(dsts->ifaces,params->rcvIface);
        current = iface->IPv6s;

        for (; current != NULL; current = current->next) {
                if (current->ipType == NOIP)
                        continue;
                if (offset + pktSz + uSz + AAAARECORDSZ > params->pktBuffSz) {
                        params->head->TC = 1;
                        break;
                }
                memcpy(pktBuff,params->namePtr,uSz);
                pktBuff += uSz;
                memcpy(pktBuff,current->AAAARECORD,AAAARECORDSZ);
                anCount++;
                pktBuff += AAAARECORDSZ;
                pktSz += uSz + AAAARECORDSZ;
        }
        params->head->ANCOUNT += anCount;
        return pktSz;
}

PRIVATE int __attachAAARecord(PKTPARAMS *params, DSTRUCTURE *dsts, int offset)
{
    /*
     * Query came from IPv6, so order does matter
     * Returns the number of bytes wrote into the buffer
     */

        U_SHORT uSz;
        U_CHAR *pktBuff;
        NETIFACE *iface;
        NETIFIPV6 *current;
        int pktSz, anCount;

        pktSz = 0;
        anCount = 0;
        uSz = sizeof(U_SHORT);
        pktBuff = params->pktBuff + offset;
        iface = getNetIfNodeByIndex(dsts->ifaces,params->rcvIface);
        current = iface->IPv6s;

        for (; current != NULL; current = current->next) {
                if (current->ipType != params->ipType)
                        continue;
                if (offset + pktSz + uSz + AAAARECORDSZ > params->pktBuffSz) {
                        params->head->TC = 1;
                        break;
                }
                memcpy(pktBuff,params->namePtr,uSz);
                pktBuff += uSz;
                memcpy(pktBuff,current->AAAARECORD,AAAARECORDSZ);
                anCount++;
                pktBuff += AAAARECORDSZ;
                pktSz += uSz + AAAARECORDSZ;
        }
        current = iface->IPv6s;
        for (; current != NULL; current = current->next) {
                if (current->ipType == NOIP)
                        continue;
                if (current->ipType == params->ipType)
                        continue;
                if (offset + pktSz + uSz + AAAARECORDSZ > params->pktBuffSz) {
                        params->head->TC = 1;
                        break;
                }
                memcpy(pktBuff,params->namePtr,uSz);
                pktBuff += uSz;
                memcpy(pktBuff,current->AAAARECORD,AAAARECORDSZ);
                anCount++;
                pktBuff += AAAARECORDSZ;
                pktSz += uSz + AAAARECORDSZ;
        }
        params->head->ANCOUNT += anCount;
        return pktSz;
}

PRIVATE int attachOtherecord(PKTPARAMS *params, DSTRUCTURE *dsts, int offset)
{
    /*
     * Fetch the requested resource record (other than 'A', 'AAAA' or
     * 'PTR')
     * Returns the number of bytes wrote into the buffer
     */
        RRLIST *current;
        U_SHORT type, uSz;
        U_CHAR *pktBuff;
        int pktSz, anCount;

        if (dsts->rList == NULL)
                return SUCCESS;
        pktSz = 0;
        anCount = 0;
        uSz = sizeof(U_SHORT);
        current = dsts->rList;
        type = params->query->QTYPE;
        pktBuff = params->pktBuff + offset;

        for (; current != NULL; current = current->NEXT) {
                if (current->TYPE == NONE)
                        continue;
                if (current->TYPE == type || type == ANY) {
                        if (offset + pktSz + uSz + current->LEN > params->pktBuffSz) {
                                params->head->TC = 1;
                                break;
                        }
                        memcpy(pktBuff,params->namePtr,uSz);
                        pktBuff += uSz;
                        memcpy(pktBuff,current->RR,current->LEN);
                        anCount++;
                        pktBuff += current->LEN;
                        pktSz += uSz + current->LEN;
                }
        }
        params->head->ANCOUNT += anCount;
        return pktSz;
}

PRIVATE int attachAnyRecord(PKTPARAMS *params, DSTRUCTURE *dsts, int offset)
{
    /*
     * Fetch 'ANY' resource records. 'PTR' records aren't fetched
     * Returns the number of bytes wrote into the buffer
     */
        int pktSz;

        pktSz = 0;
        pktSz = attachARecord(params,dsts,offset);
        pktSz += attachAAAARecord(params,dsts,offset + pktSz);
        pktSz += attachOtherecord(params,dsts,offset + pktSz);
        return pktSz;
}

PRIVATE int attachPtrRecord(PKTPARAMS *params, DSTRUCTURE *dsts, int offset)
{
    /*
     * Fetch all 'PTR' records. 'PTR' records are stored into
     * a 'NAME' list. Before attaching a 'PTR' records checks
     * if the receiving interface is "name authoritative"
     * Returns the number of bytes wrote into the buffer
     */
        char flag;
        NAME *current;
        U_SHORT uSz;
        U_CHAR *pktBuff;
        int i, pktSz, anCount;

        pktSz = 0;
        anCount = 0;
        uSz = sizeof(U_SHORT);
        current = dsts->names;
        pktBuff = params->pktBuff + offset;

        for (; current != NULL; current = current->next) {
                flag = 0;
                if (current->name == NULL)
                        continue;
                for (i = 0; i < current->notAuthOnSz; i++) {
                        if (current->notAuthOn[i] == params->rcvIface) {
                                flag = 1;
                                break;
                        }
                }
                if (!flag) {
                        if (current->ptr == NULL)
                                continue;

                        if (offset + pktSz + uSz + current->ptrSz > params->pktBuffSz) {
                                params->head->TC = 1;
                                break;
                        }
                        memcpy(pktBuff,params->namePtr,uSz);
                        pktBuff += uSz;
                        memcpy(pktBuff,current->ptr,current->ptrSz);
                        anCount++;
                        pktBuff += current->ptrSz;
                        pktSz += uSz + current->ptrSz;
                }
        }
        params->head->ANCOUNT = anCount;
        return pktSz;
}

PUBLIC int sendUDPacket(PKTSND *pktSnd)
{
    /*
     * A regular sendto, except that when the layer 3
     * protocol is IPv4 then sendmsg is used to force
     * the exit interface. When IPv6 is the layer 3
     * protocol choosing an exit interface does not work.
     * Apparently the kernel will ignore it and choose
     * his own based on the destiny's IPv6 kind
     * Returns the number of bytes sent
     */
        int sent;

        sent = 0;
        if (pktSnd->to->sa_family == AF_INET)
                sent = _sendPacket(pktSnd);
        else if (pktSnd->to->sa_family == AF_INET6)
                sent = sendto(pktSnd->fd,pktSnd->pktBuff,pktSnd->pktSz,0,
                              pktSnd->to,sizeof(SA_IN6));
        return sent;
}

PUBLIC int sendTCPacket(PKTSND *pktSnd)
{
    /*
     * Returns the number of bytes sent
     */
    int n, total, bytesLeft;

    n = 0;
    total = 0;
    bytesLeft = pktSnd->pktSz;
    while (total < pktSnd->pktSz) {
            n = send(pktSnd->fd,pktSnd->pktBuff + total,bytesLeft,0);
            if (n < 0)
                    break;
            total += n;
            bytesLeft -= n;
    }
    return total;
}

PRIVATE int _sendPacket(PKTSND *pktSnd)
{
    /*
     * Using recvmsg() to force the exit interface. This
     * only works when the layer 3 protocol is IPV4
     * Returns the number of bytes sent
     */
        int sent;
        void *vPtr;
        IPV4CMSG ancData;
        struct iovec iov;
        struct msghdr msg;
        struct cmsghdr *cmsgPtr;
        struct in_pktinfo pktInfo;

        sent = 0;
        memset(&iov,0,sizeof(iov));
        memset(&msg,0,sizeof(msg));
        memset(&ancData,0,sizeof(ancData));
        memset(&pktInfo,0,sizeof(pktInfo));
        iov.iov_base = pktSnd->pktBuff;
        iov.iov_len = pktSnd->pktSz;
        msg.msg_name = (void *)pktSnd->to;
        msg.msg_namelen = sizeof(SA_IN);
        msg.msg_iov = &iov;
        msg.msg_iovlen = 1;
        msg.msg_control = ancData.buff;
        msg.msg_controllen = sizeof(ancData.buff);
        pktInfo.ipi_ifindex = pktSnd->ifIndex;
        cmsgPtr = CMSG_FIRSTHDR(&msg);
        cmsgPtr->cmsg_len = CMSG_LEN(sizeof(struct in_pktinfo));
        cmsgPtr->cmsg_level = IPPROTO_IP;
        cmsgPtr->cmsg_type = IP_PKTINFO;
        vPtr = (void *)CMSG_DATA(cmsgPtr);
        memcpy(vPtr,&pktInfo,sizeof(pktInfo));
        sent = sendmsg(pktSnd->fd,&msg,0);
        if (sent < 0)
                sent = sendto(pktSnd->fd,pktSnd->pktBuff,pktSnd->pktSz,
                              0,(SA *)pktSnd->to,sizeof(SA_IN));
        return sent;
}

PRIVATE U_SHORT flagsToShortValue(U_CHAR *flags)
{
    /*
     * This function builds the corresponding short value
     * regarding the LLMNR header flags position
     */

        U_SHORT aux , final;
        U_CHAR i, sizeActual;

        final = 0;
        sizeActual = 16;

        for (i=0; i < 10; i++) {
                if (i==1 || i==9) {
                        sizeActual = sizeActual - 4;
                        flags[i] = flags[i] & ((1 << 4) - 1);

                        if (i==9)
                                sizeActual = 0;
                } else {
                        sizeActual--;
                        flags[i] = flags[i] & 1;
                }
                aux = flags[i] << sizeActual;

                if (i==0)
                        final = aux;
                else
                        final = final | aux;
        }
        return final;
}

PRIVATE void copyShortValues(U_CHAR *dest, U_CHAR *values, int valuesSz)
{
    /*
     * Copy a string of short values into a buffer
     * This "string" of short values holds the values
     * related to a LLMNR 'HEADER'
     */
        int i, offset, u_shortSz;

        u_shortSz = sizeof(U_SHORT);
        if (testEndianness()) {
                offset = valuesSz * u_shortSz;
                memcpy(dest,values,offset);
        }
        for (i = 0; i < valuesSz * u_shortSz; i = i + u_shortSz) {
                dest[i] = values[i + 1];
                dest[i + 1] = values[i];
        }
}

PRIVATE int testEndianness()
{
    /** ********************
    * 0 --> Little-Endian *
    * !0 --> Big-Endian   *
    ***********************/
        char *ptr;
        unsigned int test;

        test = 0xFF;
        ptr = (char *)&test;
        if (ptr != 0x0)
                return SUCCESS;
        else
                return FAILURE;
}

/*PUBLIC void printHead(HEADER *head)
{
        printf("\n");
        printf("-------------------------------");
        printf("\n");
        printf("|            %d             |\n",head->ID);
        printf("-------------------------------");
        printf("\n");
        printf("|%d|  %d  |%d|%d|%d/%d|%d|%d|%d|   %d   |\n",head->QR,
               head->OPCODE,head->C,head->TC,head->T,head->Z0,head->Z1,
               head->Z2,head->Z3,head->RCODE);
        printf("-------------------------------");
        printf("\n");
        printf("|                %d            |\n",head->QDCOUNT);
        printf("-------------------------------");
        printf("\n");
        printf("|                %d            |\n",head->ANCOUNT);
        printf("-------------------------------");
        printf("\n");
        printf("|                %d            |\n",head->NSCOUNT);
        printf("-------------------------------");
        printf("\n");
        printf("|                %d            |\n",head->ARCOUNT);
        printf("-------------------------------");
        printf("\n\n");
}*/
