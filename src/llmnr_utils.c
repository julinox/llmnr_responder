/* Macros */

/* Includes */
#include <string.h>
#include <net/if.h>
#include <netinet/in.h>

/* Own includes */
#include "../include/llmnr_defs.h"
#include "../include/llmnr_utils.h"

/* Enums & Structs */

/* Private prototypes */

/* Glocal variables */

/* Functions definitions */
PUBLIC void trim(char *string , char c)
{
    /*
     * Trim 'c' from 'string'
     */
        char *ptr, *aux;

        ptr = string;
        while (*ptr) {
                if (*ptr == c) {
                        aux = ptr;
                        while (*aux) {
                                *aux = *(aux + 1);
                                aux++;
                        }
                }
                ptr++;
        }
}
PUBLIC void strToDnsStr(char *name, char *buff)
{
    /*
     * Regular string to DNS string format converter
     * Undefined behavior when strlen(name) > buffSz
     */
        unsigned int head, tail, count, stringSize;

        if (name == NULL)
                return;
        tail = 0;
        head = 1;
        count = 0;
        stringSize = strlen(name) + 1;
        if (stringSize <= 1) {
                *buff = 0x1;
                buff[1] = *name;
                return;
        }
        buff[0] = '.';
        strcpy(buff + 1,name);
        while (head < stringSize) {
                if (buff[head] == '.') {
                        buff[tail] = count;
                        tail = head;
                        count = 0;
                } else {
                        count++;
                }
                head++;
        }
        if (tail == 0)
                buff[0] = count;
        else
                buff[tail] = count;
}

PUBLIC void dnsStrToStr(char *name, char *buff)
{
    /*
     * DNS string format to regular string converter
     */
        U_CHAR i;
        char *ptr, *ptrB;
        if (name == NULL || buff == NULL)
                return;
        ptr = name;
        ptrB = buff;
        i = *ptr++;
        while (*ptr) {
                if (i <= 0) {
                        *ptrB = '.';
                        ptrB++;
                        i = *ptr++;
                }
                *ptrB = *ptr;
                ptrB++;
                ptr++;
                i--;
        }
}

PUBLIC void fillMcastDest(SA_IN *dest4, SA_IN6 *dest6)
{
    /*
     * Fill an sockaddr_in and sockaddr_in6 structure
     * to send a multicast message
     */
        if (dest4 != NULL) {
                memset(dest4,0,sizeof(SA_IN));
                dest4->sin_family = AF_INET;
                dest4->sin_port = htons(LLMNRPORT);
                fillMcastIp(&dest4->sin_addr,NULL);
        }
        if (dest6 != NULL) {
                memset(dest6,0,sizeof(SA_IN6));
                dest6->sin6_family = AF_INET6;
                dest6->sin6_port = htons(LLMNRPORT);
                fillMcastIp(NULL,&dest6->sin6_addr);
        }
}

PUBLIC int isMulticast(void *ip, void *mcastIp, int family)
{
    /*
     * Given an ip, checks if this is a LLMNR multicast
     * address (224.0.0.252 or FF02::1:3)
     */
        INADDR *i4, *m4;
        IN6ADDR *i6, *m6;

        if (ip == NULL || mcastIp == NULL)
                return FAILURE;
        if (family == AF_INET) {
                i4 = (INADDR *)ip;
                m4 = (INADDR *)mcastIp;
                return memcmp(i4,m4,sizeof(INADDR));
        } else if (family == AF_INET6) {
                i6 = (IN6ADDR *)ip;
                m6 = (IN6ADDR *)mcastIp;
                return memcmp(i6,m6,sizeof(IN6ADDR));
        }
        return FAILURE;
}

PUBLIC void fillMcastIp(INADDR *mCast4, IN6ADDR *mCast6)
{
    /*
     * Fill the global variables MCAST_IPv4 and
     * MCAST_IPv6 (224.0.0.252 & FF02::1:3)
     */

        if (mCast4 != NULL)
                mCast4->s_addr = htonl(0xE00000FC);
        if (mCast6 != NULL) {
                memset(&mCast6->s6_addr,0,sizeof(struct in6_addr));
                mCast6->s6_addr[0] = 0xFF;
                mCast6->s6_addr[1] = 0x2;
                mCast6->s6_addr[13] = 0x1;
                mCast6->s6_addr[15] = 0x3;
        }
}

PUBLIC char *_if_indexToName(unsigned int ifIndex, char *name)
{
    /*
     * Wrapper for 'if_indextoname'
     */
        return if_indextoname(ifIndex,name);
}
