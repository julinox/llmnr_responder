/** **************************************************************
 * Interface to handle bussiness related to DNS resource records *
 * Creates, add and destroy a 'RRLIST'. This list will hold      *
 * all the resource records defined in the config file (also     *
 * will hold the 'SOA' record even though this is automatically  *
 * derivated). 'A' and 'AAAA' records will be hold into          *
 * 'ARECORD' and 'AAAARECORD' (See llmnr_net_interface.h)        *
 * respectively. 'PTR' records will be hold into 'ptr' (See      *
 * llmnr_names.h)                                                *
 * Build and store (in their respective buffer) all types        *
 * of resource records and "PTR names" (reverse lookup)          *
 *****************************************************************/

#ifndef LLMNR_RR_H
#define LLMNR_RR_H

#define RRTTL 30
#define INCLASS 1

enum RRTYPE {
        NONE = 0,
        A = 1,
        NS = 2,
        CNAME = 5,
        SOA = 6,
        PTR = 12,
        HINFO = 13,
        MX = 15,
        TXT = 16,
        AAAA = 28,
        SRV = 33,
        ANY = 255
};

typedef struct rrNode{
        int LEN;
        int TYPE;
        U_CHAR *RR;
        struct rrNode *NEXT;
} RRLIST;

typedef struct {
    /*
     * Resource record generic type
     * 'RDATA' will point to a 'TYPE' struct
     * This struct will hold a partial resource record
     */
        unsigned short TYPE;
        unsigned short CLASS;
        int TTL;
        unsigned short RDLENGTH;
        void *RDATA;
} RESRECORD;

typedef struct {
        U_CHAR IPV4[IPV4LEN];
} A_RR;

typedef struct {
        U_CHAR IPV6[IPV6LEN];
} AAAA_RR;

typedef struct {
        char *CNAME;
} CNAME_RR;

typedef struct {
        short PREFERENCE;
        char *EXCHANGE;
} MX_RR;

typedef struct {
        char *NSDNAME;
} NS_RR;

typedef struct {
        char *MNAME;
        char *RNAME;
        unsigned int SERIAL;
        unsigned int REFRESH;
        unsigned int RETRY;
        unsigned int EXPIRE;
        unsigned int MINIMUN;
} SOA_RR;


typedef struct {
        char *TXTDATA;
} TXT_RR;

typedef struct {
        char *SRV;
        char *PROTO;
        unsigned short PRIORITY;
        unsigned short WEIGHT;
        unsigned short PORT;
        char *TARGET;
} SRV_RR;

typedef struct {
        char *CPU;
        char *OS;
} HINFO_RR;

typedef struct {
        char *PTRDNAME;
} PTR_RR;

PUBLIC RRLIST *rrListHead();
PUBLIC void printRList(RRLIST *list);
PUBLIC void fillPtrRecord(NAME *names);
PUBLIC void buildARecord(INADDR *ipv4, U_CHAR *aRec, char *ptrRec);
PUBLIC void buildAAAARecord(IN6ADDR *ipv6, U_CHAR *aaaaRec, char *ptrRec);
PUBLIC void getType(int type, char *buff);
PUBLIC int newResRecord(RRLIST *list, RESRECORD *resRec);
PUBLIC int deleteRList(RRLIST **list);

#endif
