/** **************************************************
 * Linked list interface to handle a list of strings *
 *****************************************************/

#ifndef LLMNR_STR_LIST_H
#define LLMNR_STR_LIST_H

typedef struct stringNode {
        char *string;
        struct stringNode *next;
} STRNODE;

PUBLIC STRNODE *strNodeHead();
PUBLIC STRNODE *split(char *str, char delimiter);
PUBLIC char *getNodeAt(STRNODE *strList, int position);
PUBLIC void printStrNodes(STRNODE *strList);
PUBLIC void deleteStrNodeList(STRNODE **strList);
PUBLIC int countStrNodes(STRNODE *strList);
PUBLIC int newStrNode(char *string, STRNODE *strList);

#endif
