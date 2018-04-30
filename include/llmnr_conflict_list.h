/** **************************************************************
 * Linked list interface to store the conflict messages received *
 * from other peers.                                             *
 *****************************************************************/

#ifndef LLMNR_CONFLICT_LIST_H
#define LLMNR_CONFLICT_LIST_H

typedef struct __conflict {
        NAME *cName;
        int type;
        int family;
        char *logPath;
        void *cPeerIp;
        int ifIndex;
        struct tm cTime;
        struct __conflict *next;
} CONFLICT;

PUBLIC CONFLICT *conflictListHead();
PUBLIC CONFLICT *getNextConflict(CONFLICT *conflicts);
PUBLIC int addConflict(NAME *name, int type, SA_STORAGE *peer, int ifIndex, CONFLICT *conflicts);
PUBLIC int countConflicts(CONFLICT *conflicts);
PUBLIC void deleteConflictList(CONFLICT **conflicts);
PUBLIC void remConflict(CONFLICT *conflict, CONFLICT *conflicts);
PUBLIC void printConflict(CONFLICT *conflict);
PUBLIC void printConflicts(CONFLICT *conflicts);
PUBLIC void logConflict(CONFLICT *conflict, char *filePath);

#endif
