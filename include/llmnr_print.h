/** ****************************************
 * Interface to handle the daemon printing *
 * Mainly used for debugging               *
 *******************************************/

#ifndef LLMNR_PRINT_H
#define LLMNR_PRINT_H

PUBLIC void closeStream();
PUBLIC void printToStream(const char *fmt, ...);
PUBLIC void printBytes(const void *object, int size);
PUBLIC void nPrintBytes(const void *object, int size);
PUBLIC void setStream(const char *path);

#endif

