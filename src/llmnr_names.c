/* Macros */

/* Includes */
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>

/* Own includes */
#include "../include/llmnr_defs.h"
#include "../include/llmnr_print.h"
#include "../include/llmnr_names.h"

/* Private prototypes */

/* Glocal variables */

/* Functions definitions */
PUBLIC NAME *nameListHead()
{
        NAME *head;

        head = calloc(1,sizeof(NAME));
        if (head == NULL)
                return NULL;
        head->name = NULL;
        head->ptr = NULL;
        head->ptrSz = 0;
        head->next = NULL;
        head->nameStatus = UNDEFINEDNAME;
        return head;
}

PUBLIC int newName(char *name, NAME *names)
{
    /*
     * Adds a new hostname into 'NAME' list. the new 'Name'
     * will hold the status 'TENTATIVE'. Which means the
     * hostname is yet to be query in the network (checks if
     * is allready taken)
     */
        int nameLen;
        NAME *current, *previous, *newNode;

        if (names == NULL)
                return SUCCESS;
        newNode = calloc(1,sizeof(NAME));
        if (newNode == NULL)
                return SYSFAILURE;
        current = names;
        previous = current;
        for (; current != NULL; current = current->next) {
                if (current->name == NULL)
                        continue;
                if (!strcmp(name,current->name))
                        return FAILURE;
                previous = current;
        }
        nameLen = strlen(name) + 1;
        newNode->name = calloc(1,nameLen);
        if (newNode->name == NULL) {
                free(newNode);
                return SYSFAILURE;
        }
        newNode->nameStatus = TENTATIVE;
        newNode->notAuthOnSz = 0;
        newNode->ptr = NULL;
        newNode->ptrSz = 0;
        strcpy(newNode->name,name);
        newNode->next = previous->next;
        previous->next = newNode;
        return SUCCESS;
}

PUBLIC void deleteNameList(NAME **names)
{
        NAME *current, *dispose;

        if (*names == NULL)
                return;
        current = *names;
        while (current != NULL) {
                dispose = current;
                current = current->next;
                if (dispose->name != NULL)
                        free(dispose->name);
                if (dispose->ptr != NULL)
                        free(dispose->ptr);
                free(dispose);
        }
        *names = NULL;
}

PUBLIC int nameListSz(NAME *names)
{
        int count;
        NAME *current;

        count = 0;
        if (names == NULL)
                return count;
        current = names;
        for (; current != NULL; current = current->next)
                count++;
        return count;
}

PUBLIC NAME *getNameNodeByName(char *name, NAME *names)
{
        NAME *current;

        if (names == NULL)
                return NULL;
        current = names;
        for (; current != NULL; current = current->next ) {
                if (current->name == NULL)
                        continue;
                if (!strncasecmp(current->name,name,strlen(current->name)))
                        return current;
        }
        return NULL;
}

PUBLIC void addAuthOn(NAME *name, int ifIndex)
{
    /*
     * Add an interface number into 'authOn' array
     */
        int i;

        for (i = 0; i < name->authOnSz; i++) {
                if (name->authOn[i] == ifIndex)
                        return;
        }
        name->authOn[name->authOnSz++] = ifIndex;
}

PUBLIC void delAuthOn(NAME *name, int ifIndex)
{
    /*
     * Delete an interface number from 'authOn' array
     */
        U_CHAR *ptr;
        int i, mark;

        mark = -1;
        ptr = name->authOn;
        for (i = 0; i < name->authOnSz; i++) {
                if (mark >= 0) {
                        ptr[mark++] = ptr[i];
                        ptr[i] = 0;
                }
                if (ptr[i] == ifIndex) {
                        mark = i;
                        if (name->authOnSz == 1) {
                                ptr[i] = 0;
                                break;
                        }
                }
        }
        if (mark >= 0)
                name->authOnSz--;
}

PUBLIC void addNotAuthOn(NAME *name, int ifIndex)
{
    /*
     * Add an interface number into 'notAuthOn' array
     */
        int i;

        for (i = 0; i < name->notAuthOnSz; i++) {
                if (name->notAuthOn[i] == ifIndex)
                        return;
        }
        name->notAuthOn[name->notAuthOnSz++] = ifIndex;
}

PUBLIC void delNotAuthOn(NAME *name, int ifIndex)
{
    /*
     * Delete an interface number from 'notAuthOn' array
     */
        U_CHAR *ptr;
        int i, mark;

        mark = -1;
        ptr = name->notAuthOn;
        for (i = 0; i < name->notAuthOnSz; i++) {
                if (mark >= 0) {
                        ptr[mark++] = ptr[i];
                        ptr[i] = 0;
                }
                if (ptr[i] == ifIndex) {
                        mark = i;
                        if (name->notAuthOnSz == 1) {
                                ptr[i] = 0;
                                break;
                        }
                }
        }
        if (mark >= 0)
                name->notAuthOnSz--;
}

PUBLIC int isNotAuthOn(NAME *name, int ifIndex)
{
    /*
     * Checks if a given interface number is into 'notAuthOn'
     */
        int i;

        if (name == NULL)
                return SUCCESS;
        for (i = 0; i < name->notAuthOnSz; i++) {
                if (name->notAuthOn[i] == ifIndex)
                        return SUCCESS;
        }
        return FAILURE;
}

PUBLIC void printNames(NAME *names)
{
        int i;
        NAME *current;

        if (names == NULL)
                return;
        current = names;
        for (; current != NULL; current = current->next) {
                if (current->name != NULL)
                        nPrintBytes(current->name,strlen(current->name));
                if (current->ptr != NULL)
                        printBytes(current->ptr,current->ptrSz);
                switch (current->nameStatus) {
                case OWNER: printToStream("Owner\n"); break;
                case TENTATIVE: printToStream("Tentative\n"); break;
                case UNDEFINEDNAME: printToStream("Undefined\n"); break;
                }
                printToStream("ptrsz: %d. ",current->ptrSz);
                printBytes(current->ptr,current->ptrSz);

                printToStream("authsz = %d:  ",current->authOnSz);
                for (i = 0; i < current->authOnSz; i++)
                        printToStream("%d, ",current->authOn[i]);
                printToStream("\n");
                printToStream("notAuthsz = %d:  ",current->notAuthOnSz);
                for (i = 0; i < current->notAuthOnSz; i++)
                        printToStream("%d, ",current->notAuthOn[i]);
                printToStream("\n----------------------------------\n");
        }
}
