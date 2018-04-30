/** ****************************************************************
 * Linked list interface to handle the hostnames for which a host  *
 * is authoritative. A machine can be authoritative on several     *
 * hostnames (See RFC 4795).                                       *
 * The struct 'NAME' will hold an actual hostname and two arrays:  *
 * - 'notAuthOn' holds the interfaces numbers that aren't          *
 *   authoritative on that specific name (i.e name was taken)      *
 * - 'authOn' holds the interfaces numbers that are                *
 *   authoritative on that specific name. Having 'notAuthOn' this  *
 *   array ('authOn') seems unnecesary, but is needed for 'cDar'   *
 * Note: In LLMNR a machine can be authoritative for several       *
 * hostnames                                                       *
 *******************************************************************/

#ifndef LLMNR_NAMES_H
#define LLMNR_NAMES_H

enum NAMESTATUS {
        OWNER,
        TENTATIVE,
        UNDEFINEDNAME
};

typedef struct nameNode {
        char *name;
        char nameStatus;
        int ptrSz;
        U_CHAR *ptr;
        U_SHORT authOnSz;
        U_SHORT notAuthOnSz;
        U_CHAR authOn[MAXIFACES];
        U_CHAR notAuthOn[MAXIFACES];
        struct nameNode *next;
} NAME;

PUBLIC NAME *nameListHead();
PUBLIC NAME *getNameNodeByName(char *name, NAME *names);
PUBLIC void printNames(NAME *names);
PUBLIC void deleteNameList(NAME **names);
PUBLIC void addAuthOn(NAME *name, int ifIndex);
PUBLIC void delAuthOn(NAME *name, int ifIndex);
PUBLIC void addNotAuthOn(NAME *name, int ifIndex);
PUBLIC void delNotAuthOn(NAME *name, int ifIndex);
PUBLIC int newName(char *name, NAME *names);
PUBLIC int nameListSz(NAME *names);
PUBLIC int isNotAuthOn(NAME *name, int ifIndex);

#endif
