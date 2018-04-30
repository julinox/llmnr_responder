/* Macros */
#define INTSZ 4
#define USHORTSZ 2
#define RRPARTIALSZ 10

/* Includes */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netinet/in.h>

/* Own includes */
#include "../include/llmnr_defs.h"
#include "../include/llmnr_names.h"
#include "../include/llmnr_print.h"
#include "../include/llmnr_utils.h"
#include "../include/llmnr_rr.h"

/* Private prototypes */
PRIVATE void copyRR(U_CHAR *RR, RESRECORD *resRec);
PRIVATE void copyRdata(int type, U_CHAR *rDataPtr, void *rData, int rDataLen);
PRIVATE void fillPtrRR(int type, void *ip, char *ptrRec);
PRIVATE void hexToAsciiPtr(U_CHAR byte, char *ret);
PRIVATE void printResRecord(U_CHAR *rr, int rrLen);
PRIVATE void printType(int type);
PRIVATE U_CHAR hexToAsciiPtrH(U_CHAR nibble);

/* Glocal variables */

/* Function definitions */
PUBLIC RRLIST *rrListHead()
{
    /*
     * 'RRLIST' head
     */
        RRLIST *head;

        head = calloc(1,sizeof(RRLIST));
        if (head == NULL)
                return NULL;
        head->LEN = 0;
        head->TYPE = NONE;
        head->RR = NULL;
        head->NEXT = NULL;
        return head;
}

PUBLIC int newResRecord(RRLIST *list, RESRECORD *resRec)
{
    /*
     * New 'RRLIST' node
     */
        RRLIST *aux, *newNode;

        if (list == NULL || resRec == NULL)
                return SUCCESS;
        newNode = calloc(1,sizeof(RRLIST));
        if (newNode == NULL)
                return FAILURE;
        aux = list;
        while (aux->NEXT != NULL)
                aux = aux->NEXT;
        newNode->RR = calloc(1,RRPARTIALSZ + resRec->RDLENGTH);
        if (newNode->RR == NULL) {
                free(newNode);
                return FAILURE;
        }
        copyRR(newNode->RR,resRec);
        copyRdata(resRec->TYPE,newNode->RR + RRPARTIALSZ,resRec->RDATA,
                  resRec->RDLENGTH);
        newNode->LEN = RRPARTIALSZ + resRec->RDLENGTH;
        newNode->TYPE = resRec->TYPE;
        newNode->NEXT = NULL;
        aux->NEXT = newNode;
        return SUCCESS;
}

PUBLIC int deleteRList(RRLIST **list)
{
        RRLIST *aux, *dispose;

        if (*list == NULL)
                return SUCCESS;
        aux = *list;
        while (aux->NEXT != NULL) {
                dispose = aux;
                aux = aux->NEXT;
                free(dispose->RR);
                free(dispose);
        }
        free(aux->RR);
        free(aux);
        *list = NULL;
        return SUCCESS;
}

PUBLIC void buildARecord(INADDR *ipv4, U_CHAR *aRec, char *ptrRec)
{
    /*
     * Given an IPv4, build his derivated 'A' 'RESRECORD'
     * Also derives the associated 'PTR' "name" (See RFC 1035)
     * 'aRec' is a pointer to 'ARECORD'
     * 'ptrRec' is a pointer to 'PTR4RECORD'
     * (See llmnr_net_interface.h)
     */
        A_RR arr;
        RESRECORD rr;

        if (aRec == NULL)
                return;
        memcpy(arr.IPV4,ipv4,IPV4LEN);
        rr.TYPE = A;
        rr.CLASS = INCLASS;
        rr.TTL = RRTTL;
        rr.RDLENGTH = IPV4LEN;
        rr.RDATA = (void *)&arr;
        copyRR(aRec,&rr);
        copyRdata(A,aRec + RRPARTIALSZ,&arr,IPV4LEN);
        fillPtrRR(AF_INET,ipv4,ptrRec);
}

PUBLIC void buildAAAARecord(IN6ADDR *ipv6, U_CHAR *aaaaRec, char *ptrRec)
{
    /*
     * Same thing that 'buildARecord' but for IPv6 instead
     */
        AAAA_RR aaaarr;
        RESRECORD rr;

        if (aaaaRec == NULL)
                return;
        memcpy(aaaarr.IPV6,ipv6,IPV6LEN);
        rr.TYPE = AAAA;
        rr.CLASS = INCLASS;
        rr.TTL = RRTTL;
        rr.RDLENGTH = IPV6LEN;
        rr.RDATA = (void *)&aaaarr;
        copyRR(aaaaRec,&rr);
        copyRdata(AAAA,aaaaRec + RRPARTIALSZ,&aaaarr,IPV6LEN);
        fillPtrRR(AF_INET6,ipv6->s6_addr,ptrRec);
}

PUBLIC void fillPtrRecord(NAME *names)
{
    /*
     * For every node of a 'NAME' list, build his
     * corresponding (and partial) 'PTR' resource record
     */
        int len;
        RESRECORD rr;
        NAME *current;
        PTR_RR ptrrr;

        if (names == NULL)
                return;
        current = names;
        rr.TYPE = PTR;
        rr.CLASS = INCLASS;
        rr.TTL = RRTTL;

        for (; current != NULL; current = current->next) {
                if (current->name == NULL)
                        continue;
                len = strlen(current->name) + 1;
                current->ptrSz = len + RRPARTIALSZ;
                current->ptr = calloc(1,current->ptrSz);
                if (current->ptr == NULL) {
                        current->ptrSz = 0;
                        continue;
                }
                rr.RDLENGTH = len;
                ptrrr.PTRDNAME = current->name;
                rr.RDATA = (void *)&ptrrr;
                copyRR(current->ptr,&rr);
                copyRdata(PTR,current->ptr + RRPARTIALSZ,&ptrrr,len);
        }
}

PUBLIC void getType(int type, char *buff)
{
    /*
     * Given a type returns the corresponding
     * string value for that int
     */
        if (buff == NULL)
                return;
        switch (type) {
        case A: strcpy(buff,"A"); break;
        case AAAA: strcpy(buff,"AAAA"); break;
        case NS: strcpy(buff,"NS"); break;
        case CNAME: strcpy(buff,"CNAME"); break;
        case SOA: strcpy(buff,"SOA"); break;
        case PTR: strcpy(buff,"PTR"); break;
        case MX: strcpy(buff,"MX"); break;
        case TXT: strcpy(buff,"TXT"); break;
        case SRV: strcpy(buff,"SRV"); break;
        case ANY: strcpy(buff,"ANY"); break;
        }
}

PUBLIC void printRList(RRLIST *list)
{
        RRLIST *aux;

        if (list == NULL)
                return;
        aux = list;
        for (; aux != NULL; aux = aux->NEXT) {
                if (aux->RR != NULL) {
                        printToStream("Len: %d\n",aux->LEN);
                        printResRecord(aux->RR,aux->LEN);
                        printToStream("-------------------------------------------\n");
                }
        }
}

PRIVATE void copyRR(U_CHAR *RR, RESRECORD *resRec)
{
    /*
     * Copy a 'RESRECORD' struct into a buffer.
     * Note: This will copy a partial resource record
     * ('TYPE','CLASS','TTL' and 'RDLENGTH') into a buffer (in
     * network byte order)
     */
        int aux2;
        U_SHORT aux1;

        aux1 = htons(resRec->TYPE);
        memcpy(RR,&aux1,sizeof(short));
        aux1 = htons(resRec->CLASS);
        memcpy(RR + sizeof(short),&aux1,sizeof(short));
        aux2 = htonl(resRec->TTL);
        memcpy(RR + 2 * sizeof(short),&aux2,sizeof(int));
        aux1 = htons(resRec->RDLENGTH);
        memcpy(RR + sizeof(int) + 2 * sizeof(short),&aux1,sizeof(short));
}

PRIVATE void copyRdata(int type, U_CHAR *rDataPtr, void *rData, int rDataLen)
{
    /*
     * Copy the 'RDATA' part belonging to a resource record into 'rDataPtr'
     * (the copy is made in network byte order). 'rData' is a pointer to a
     * struct that holds the 'RDATA' value. 'type' specifies the 'RRTYPE'
     * Note: Remember that 'RDATA' and 'NAME' are variables in a resource
     * record (See RFC 1035)
     */
        short auxshort;
        int soaPtr, intaux;
        U_SHORT ushortaux;
        unsigned int uintaux;

        A_RR *arr;
        AAAA_RR *aaaarr;
        MX_RR *mxrr;
        NS_RR *nsrr;
        CNAME_RR *cnamerr;
        SOA_RR *soarr;
        TXT_RR *txtrr;
        HINFO_RR *hinforr;
        SRV_RR *srvrr;
        PTR_RR *ptrrr;

        if (rData == NULL)
                return;
        switch (type) {

        case A:
                arr = (A_RR *)rData;
                memcpy(rDataPtr,arr->IPV4,IPV4LEN);
                break;
        case AAAA:
                aaaarr = (AAAA_RR *)rData;
                memcpy(rDataPtr,aaaarr->IPV6,IPV6LEN);
                break;
        case CNAME:
                cnamerr = (CNAME_RR *)rData;
                strcpy((char *)rDataPtr,cnamerr->CNAME);
                break;
        case NS:
                nsrr = (NS_RR *)rData;
                strcpy((char *)rDataPtr,nsrr->NSDNAME);
                break;
        case MX:
                mxrr = (MX_RR *)rData;
                auxshort = htons(mxrr->PREFERENCE);
                memcpy(rDataPtr,&auxshort,sizeof(short));
                strcpy((char *)rDataPtr + sizeof(short),mxrr->EXCHANGE);
                break;
        case SOA:
                soarr = (SOA_RR *)rData;
                soaPtr = 2 * USHORTSZ;
                memcpy(rDataPtr,soarr->MNAME,USHORTSZ);
                memcpy(rDataPtr + USHORTSZ,soarr->RNAME,USHORTSZ);
                uintaux = htonl(soarr->SERIAL);
                memcpy(rDataPtr + soaPtr, &uintaux, INTSZ);
                intaux = htonl(soarr->REFRESH);
                memcpy(rDataPtr + soaPtr + 1 * INTSZ,&intaux,INTSZ);
                intaux = htonl(soarr->RETRY);
                memcpy(rDataPtr + soaPtr + 2 * INTSZ,&intaux,INTSZ);
                intaux = htonl(soarr->EXPIRE);
                memcpy(rDataPtr + soaPtr + 3 * INTSZ,&intaux,INTSZ);
                intaux = htonl(soarr->MINIMUN);
                memcpy(rDataPtr + soaPtr + 4 * INTSZ,&intaux,INTSZ);
                break;
        case TXT:
                txtrr = (TXT_RR *)rData;
                memcpy(rDataPtr,txtrr->TXTDATA,rDataLen);
                break;
        case HINFO:
                hinforr = (HINFO_RR *)rData;
                strcpy((char *)rDataPtr,hinforr->CPU);
                strcpy((char *)rDataPtr + strlen(hinforr->CPU) + 1,
                       hinforr->OS);
                break;
        case SRV:
                srvrr = (SRV_RR *)rData;
                ushortaux = htons(srvrr->PRIORITY);
                memcpy(rDataPtr,&ushortaux,sizeof(U_SHORT));
                ushortaux = htons(srvrr->WEIGHT);
                memcpy(rDataPtr + sizeof(U_SHORT),&ushortaux,
                       sizeof(U_SHORT));
                ushortaux = htons(srvrr->PORT);
                memcpy(rDataPtr + 2 * sizeof(U_SHORT),&ushortaux,
                       sizeof(U_SHORT));
                strcpy((char *)rDataPtr + 3 * sizeof(U_SHORT),srvrr->TARGET);
                break;
        case PTR:
                ptrrr = (void *)rData;
                strcpy((char *)rDataPtr,ptrrr->PTRDNAME);
                break;
        }
}

PRIVATE void fillPtrRR(int type, void *ip, char *ptrRec)
{
    /*
     * Given an ip of type 'type' (IPv4 or IPv6) build his
     * corresponding PTR name (See RFC 1035) and store it
     * into 'ptrRec'
     *
     */
        int i;
        char str[16];
        U_CHAR *ptr;
        char *aux, auxStr[75];
        char newName[HOSTNAMEMAX + 1];
        char domain4[] = "in-addr.arpa.";
        char domain6[] = "ip6.arpa.";

    /* IPvX its in network order */
        memset(auxStr,0,75);
        memset(newName,0,HOSTNAMEMAX + 1);
        ptr = (U_CHAR *)ip;

        switch (type) {

        case AF_INET:
                for (i = IPV4LEN - 1; i >= 0; i--) {
                        memset(str,0,16);
                        sprintf(str, "%d", (ptr[i]));
                        strcat(auxStr,str);
                        strcat(auxStr,".");
                }
                strcpy(newName,auxStr);
                strcat(newName,domain4);
                break;

        case AF_INET6:
                aux = auxStr;
                for (i = IPV6LEN - 1; i >=0; i--) {
                        hexToAsciiPtr(ptr[i],aux);
                        aux[1] = '.';
                        aux[3] = '.';
                        aux += 4;
                }
                strcpy(newName,auxStr);
                strcat(newName,domain6);
                break;
        }
        strToDnsStr(newName,ptrRec);
}

PRIVATE void hexToAsciiPtr(U_CHAR byte, char *ret)
{
    /*
     * Special hex to ascii (PTR name only)
     * Given a byte will halve it, exchange it and turn it
     * into two chars separeted by a "."
     * Example: 0xAB -> "B.A"
     */
       U_CHAR highNibble, lowNibble;

        highNibble = byte >> 4;
        lowNibble = byte & 0x0F;
        ret[0] = hexToAsciiPtrH(lowNibble);
        ret[2] = hexToAsciiPtrH(highNibble);
}

PRIVATE U_CHAR hexToAsciiPtrH(U_CHAR nibble)
{
    /*
     * Helper function for 'hexToAsciiPtr'
     * Given a hex number will turn it into ascii character
     */
        U_CHAR ret;

        switch (nibble) {
        case 0x0:
        case 0x1:
        case 0x2:
        case 0x3:
        case 0x4:
        case 0x5:
        case 0x6:
        case 0x7:
        case 0x8:
        case 0x9:
                ret = nibble + 48;
                break;
        case 0xA:
        case 0xB:
        case 0xC:
        case 0xD:
        case 0xE:
        case 0xF:
                ret = nibble + 87;
                break;
        }
        return ret;
}

PRIVATE void printResRecord(U_CHAR *rr, int rrLen)
{
        RESRECORD aux;

        aux.TYPE = ntohs(*((U_SHORT *)rr));
        aux.CLASS = ntohs(*((U_SHORT *)(rr + sizeof(U_SHORT))));
        aux.TTL = ntohl(*((int *)(rr + 2 * sizeof(U_SHORT))));
        aux.RDLENGTH = ntohs(*((U_SHORT *)(rr + sizeof(int) + 2 * sizeof(U_SHORT))));
        printToStream("Type: ");
        printType(aux.TYPE);
        printToStream("Class: %d\n",(aux.CLASS));
        printToStream("TTL: %d\n",(aux.TTL));
        printToStream("RDLENGTH: %d\n",(aux.RDLENGTH));
        printBytes(rr,rrLen);
}

PRIVATE void printType(int type)
{
        switch (type) {
        case NONE: printToStream("None\n"); break;
        case A: printToStream("A\n"); break;
        case AAAA: printToStream("AAAA\n"); break;
        case NS: printToStream("NS\n"); break;
        case CNAME: printToStream("CNAME\n"); break;
        case SOA: printToStream("SOA\n"); break;
        case PTR: printToStream("PTR\n"); break;
        case MX: printToStream("MX\n"); break;
        case TXT: printToStream("TXT\n"); break;
        case SRV: printToStream("SRV\n"); break;
        case HINFO: printToStream("HINFO\n"); break;
        default: printToStream("No type: %d\n",type);
        }
}
