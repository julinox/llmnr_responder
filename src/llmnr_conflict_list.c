/* Macros */
#define MAXCONFLICTS 1 + 5

/* Includes */
#include <time.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>

/* Own includes */
#include "../include/llmnr_defs.h"
#include "../include/llmnr_names.h"
#include "../include/llmnr_rr.h"
#include "../include/llmnr_utils.h"
#include "../include/llmnr_syslog.h"
#include "../include/llmnr_conflict_list.h"

#include "../include/llmnr_print.h"
/* Enums & Structs */

/* Private functions prototypes */
PRIVATE void writeLog(char *str, char *filePath);

/* Glocal variables */

/* Functions definitions */
PUBLIC CONFLICT *conflictListHead()
{
    /*
     * List head
     */
        CONFLICT *head;

        head = calloc(1,sizeof(CONFLICT));
        if (head == NULL)
                return NULL;
        head->cName = NULL;
        head->type = -1;
        head->family = 0;
        head->logPath = NULL;
        head->cPeerIp = NULL;
        head->next = NULL;
        return head;
}

PUBLIC int addConflict(NAME *name, int type, SA_STORAGE *peer, int ifIndex, CONFLICT *conflicts)
{
    /*
     * Add conflict to the list. Time specifies the time when the
     * message was received (is stored in localtime)
     * MAXCONFLICTS limits the number of conflicts stored
     */
        time_t tm;
        CONFLICT *current, *previous, *newNode;

        if (conflicts == NULL)
                return SUCCESS;
        if (name == NULL)
                return SUCCESS;
        if (peer == NULL)
                return SUCCESS;
        if (countConflicts(conflicts) > MAXCONFLICTS)
                return SUCCESS;
        newNode = calloc(1,sizeof(CONFLICT));
        if (newNode == NULL)
                return FAILURE;
        current = conflicts;
        for (; current != NULL; current = current->next)
                previous = current;

        if (peer->ss_family == AF_INET) {
                newNode->cPeerIp = calloc(1,sizeof(INADDR));
                if (newNode->cPeerIp == NULL) {
                        free(newNode);
                        return FAILURE;
                }
                newNode->family = AF_INET;
                memcpy(newNode->cPeerIp,&((SA_IN *)peer)->sin_addr,
                       sizeof(INADDR));

        } else if (peer->ss_family == AF_INET6) {
                newNode->cPeerIp = calloc(1,sizeof(IN6ADDR));
                if (newNode->cPeerIp == NULL) {
                        free(newNode->cPeerIp);
                        free(newNode);
                        return FAILURE;
                }
                newNode->family = AF_INET6;
                memcpy(newNode->cPeerIp,&((SA_IN6 *)peer)->sin6_addr,
                       sizeof(IN6ADDR));
        }
        newNode->cName = name;
        newNode->type = type;
        newNode->logPath = NULL;
        newNode->ifIndex = ifIndex;
        time(&tm);
        localtime_r(&tm,&newNode->cTime);
        previous->next = newNode;
        newNode->next = NULL;
        return SUCCESS;
}

PUBLIC void remConflict(CONFLICT *conflict, CONFLICT *conflicts)
{
    /*
     * Remove conflict from list
     */
        CONFLICT *current, *dispose, *previous;

        if (conflicts == NULL || conflict == NULL)
                return;
        current = conflicts;
        while (current != NULL) {
                if (current == conflict) {
                        dispose = current;
                        previous->next = dispose->next;
                        free(dispose->cPeerIp);
                        free(dispose);
                        break;
                }
                previous = current;
                current = current->next;
        }
}

PUBLIC void deleteConflictList(CONFLICT **conflicts)
{
        CONFLICT *current, *dispose;

        if (conflicts == NULL)
                return;
        current = *conflicts;
        while (current != NULL) {
                dispose = current;
                current = current->next;
                if (dispose->cPeerIp != NULL)
                        free(dispose->cPeerIp);
                if (dispose->logPath != NULL)
                        free(dispose->logPath);
                free(dispose);
        }
        *conflicts = NULL;
}

PUBLIC int countConflicts(CONFLICT *conflicts)
{
        int i;
        CONFLICT *current;

        i = 0;
        if (conflicts == NULL)
                return i;
        current = conflicts;
        for (; current != NULL; current = current->next)
                i++;
        return i;
}

PUBLIC CONFLICT *getNextConflict(CONFLICT *conflicts)
{
    /*
     * Returns the first conflict in the list, to be treated
     */
        CONFLICT *current;

        if (conflicts == NULL)
                return NULL;
        current = conflicts;
        while (current != NULL) {
                if (current->type > 0)
                        return current;
                current = current->next;
        }
        return NULL;
}

PUBLIC void logConflict(CONFLICT *conflict, char *filePath)
{
    /*
     * Log a conflict in the conflict log. RFC 4795 requires conflicts
     * to be logged
     */
        int sz;
        char *string, *name, type[10];
        char ipStr[INET6_ADDRSTRLEN + 1];

        sz = 20 + sizeof(int) * 6;
        if (conflict->cName == NULL)
                return;
        if (conflict->cName->name == NULL)
                return;
        if (filePath == NULL || conflict->cPeerIp == NULL)
                return;
        memset(type,0,sizeof(type));
        memset(ipStr,0,INET6_ADDRSTRLEN);
        if (inet_ntop(conflict->family,conflict->cPeerIp,ipStr,
                  INET6_ADDRSTRLEN) == NULL)
                return;
        name = calloc(1,strlen(conflict->cName->name));
        if (name == NULL)
                return;
        dnsStrToStr(conflict->cName->name,name);
        getType(conflict->type,type);
        sz += strlen(ipStr) + 1;
        sz += strlen(conflict->cName->name) + 1;
        string = calloc(1,sz);
        if (string == NULL) {
                free(name);
                return;
        }
        sz = 0;
        sz = sprintf(string,"%s %s %d/%d/%d--%d:%d:%d %s",name,
               type,conflict->cTime.tm_mday,conflict->cTime.tm_mon,
               conflict->cTime.tm_year + 1900,conflict->cTime.tm_hour,
               conflict->cTime.tm_min,conflict->cTime.tm_sec,ipStr);
        if (sz)
                writeLog(string,filePath);
        free(name);
        free(string);
}

PRIVATE void writeLog(char *str, char *filePath)
{
    /*
     * Write 'str' to 'filePath'
     */
        FILE *file;

        if (!strcasecmp(filePath,"syslog")) {
                syslogConflict(str);
        } else {
                file = fopen(filePath,"a");
                if (file == NULL)
                        return;
                fprintf(file,"%s\n",str);
                fclose(file);
        }
}
