/** *********************************************
 * Interface that groups a set of miscellaneous *
 * functions                                    *
 ************************************************/

#ifndef LLMNR_UTILS_H
#define LLMNR_UTILS_H

PUBLIC void strToDnsStr(char *name, char *buff);
PUBLIC void dnsStrToStr(char *name, char *buff);
PUBLIC void fillMcastIp(INADDR *mCast4, IN6ADDR *mCast6);
PUBLIC void fillMcastDest(SA_IN *dest4, SA_IN6 *dest6);
PUBLIC void trim(char *string , char c);
PUBLIC char *_if_indexToName(unsigned int ifIndex, char *name);
PUBLIC int isMulticast(void *ip, void *mcastIp, int family);

#endif
