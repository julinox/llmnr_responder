/* Macros */

/* Includes */
#include <string.h>
#include <stdlib.h>

/* Own includes */
#include "../include/llmnr_defs.h"
#include "../include/llmnr_str_list.h"

/* Enums & Structs */

/* Private prototypes */

/* Glocal variables */

/* Functions definitions */
PUBLIC STRNODE *strNodeHead()
{
    /*
     * String list head
     */
        STRNODE *head;

        head = calloc(1,sizeof(STRNODE));
        head->string = NULL;
        head->next = NULL;
        return head;
}

PUBLIC int newStrNode(char *string, STRNODE *strList)
{
        int stringLen;
        STRNODE *current, *previous, *newNode;

        if (strList == NULL)
                return SUCCESS;
        newNode = calloc(1,sizeof(STRNODE));
        if (newNode == NULL)
                return FAILURE;
        current = strList;
        previous = current;
        for (; current != NULL; current = current->next)
                previous = current;
        stringLen = strlen(string) + 1;
        newNode->string = calloc(1,stringLen);
        if (newNode->string == NULL)
                return FAILURE;
        strcpy(newNode->string,string);
        newNode->next = previous->next;
        previous->next = newNode;
        return SUCCESS;
}

PUBLIC void deleteStrNodeList(STRNODE **strList)
{
        STRNODE *current, *dispose;

        if (*strList == NULL)
                return;
        current = *strList;
        while (current != NULL) {
                dispose = current;
                current = current->next;
                if (dispose->string != NULL)
                        free(dispose->string);
                free(dispose);
        }
        *strList = NULL;
}

PUBLIC char *getNodeAt(STRNODE *strList, int position)
{
        int i;
        STRNODE *current;

        if (strList == NULL || position < 0)
                return NULL;
        i = 0;
        current = strList;
        while (current != NULL) {
                if (i++ == position)
                        break;
                current = current->next;
        }
        return current->string;
}

PUBLIC int countStrNodes(STRNODE *strList)
{
        int i;
        STRNODE *current;

        if (strList == NULL)
                return 0;
        i = 0;
        current = strList;
        for (; current != NULL; current = current->next)
                i++;
        return i;
}

PUBLIC STRNODE *split(char *str, char delimiter)
{
    /*
     * Given a string and a delimiter this function will split
     * the string in several tokens. Returns a list of strings
     */

        int strSz;
        char *first, *last;
        STRNODE *strList;

        if ( str == NULL)
                return NULL;
        else
                strSz = strlen(str) + 1;

        char strCpy[strSz];
        strcpy(strCpy,str);
        first = strCpy;
        last = strCpy;
        strList = strNodeHead();

        if (strList == NULL)
                return NULL;
        while (*(last + 1)) {
                if(*first == delimiter){
                    first++;
                } else if (*last == delimiter) {
                        *last = 0;
                        if (newStrNode(first,strList)) {
                                deleteStrNodeList(&strList);
                                return NULL;
                        }
                        first = last + 1;
                }
                last++;
        }
        if (*first != delimiter) {
                if (*last == delimiter)
                        *last = 0;
                if (newStrNode(first,strList)) {
                        deleteStrNodeList(&strList);
                        return NULL;
                }
        }
        return strList;
}
