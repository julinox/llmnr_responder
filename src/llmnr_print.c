/* Macros */

/* Includes */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

/* Own includes */
#include "../include/llmnr_defs.h"
#include "../include/llmnr_print.h"

/* Enums & Structs */

/* Private prototypes */

/* Glocal variables */
PRIVATE char *LogPath = NULL;

/* Functions definitions */
PUBLIC void setStream(const char *path)
{
    /*
     * Set stream where to print. Default: stdout
     */
        if (path == NULL)
                return;
        LogPath = calloc(1,strlen(path) + 1);
        if (LogPath != NULL)
                strcpy(LogPath,path);
}

PUBLIC void closeStream()
{
    /*
     * Close the previously set stream
     */
        if (LogPath != NULL)
                free(LogPath);
}

PUBLIC void printToStream(const char *fmt, ...)
{
    /*
     * Print to previously set stream. Default: stdout
     */
        va_list ap;
        FILE *file;

        file = NULL;
        if (LogPath == NULL)
                file = stdout;
        else
                file = fopen(LogPath,"a");
        if (file != NULL) {
                va_start(ap,fmt);
                vfprintf(file,fmt,ap);
                va_end(ap);
        }
        if (file != NULL && file != stdout)
                fclose(file);
}

PUBLIC void printBytes(const void *object, int size)
{
    /*
     * Print a set of memory bytes
     */
        int i, hex;

        printToStream("[ ");

        for (i = 0; i < size; i++) {
                hex =  (((const unsigned char *) object)[i] & 0xff );
                printToStream("%x,",hex);
        }
        printToStream("]\n");
}

PUBLIC void nPrintBytes(const void *object, int size)
{
    /*
     * Print a set of memory bytes (only letters and numbers)
     */
        int i, hex;

        printToStream("[ ");
        for (i = 0; i < size; i++) {
                hex =  (((const unsigned char *) object)[i] & 0xff );
                if (hex == 32 || (hex > 47 && hex < 58) ||
                    (hex > 64 && hex < 91) || (hex > 96 && hex < 123))
                        printToStream("%c", hex);
                else if (hex == 0)
                        printToStream("0");
                else
                        printToStream(",");
        }
        printToStream("]\n");
}
