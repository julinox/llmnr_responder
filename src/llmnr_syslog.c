/* Macros */
#define BUFFERSZ 512
#define LOGSTRMAXLEN 458

/* Includes */
#include <syslog.h>
#include <string.h>

/* Own includes */
#include "../include/llmnr_defs.h"
#include "../include/llmnr_print.h"
#include "../include/llmnr_syslog.h"

/* Enums & Structs */

/* Private prototypes */
PRIVATE void _syslog(char *logMsg);

/* Glocal variables */
PRIVATE char Ident[] = "llmnrd";
PRIVATE int Option = LOG_PID;
PRIVATE int Level = LOG_ERR;
PRIVATE int Facility = LOG_DAEMON;
PRIVATE const char BADPARAMETER[] = "Invalid line in config file ";
PRIVATE const char NONAME[] = "None/invalid Hostname. Daemon halted!";
PRIVATE const char BADNAME[] = "Invalid name definition ";
PRIVATE const char REPEATIF[] = "Repeat / redundandt interface definition ";
PRIVATE const char TXTDEFINED[] = "TXT resoure record allready defined";
PRIVATE const char BADLOGLEVEL[] = "No such log level ";
PRIVATE const char BADLOGFACILITY[] = "No such log facility ";
PRIVATE const char LOGLEVELDEFINED[] = "LogLevel allready defined ";
PRIVATE const char LOGFACILITYDEFINED[] = "LogFacility allready defined ";
PRIVATE const char LOGFILE[] = "Cannot log conflicts on ";
PRIVATE const char CREATECONFFILE[] = "Error creating config file ";
PRIVATE const char CREATEPIDFILE[] = "Error creating pid file ";
PRIVATE const char WRITECONFIGFILE[] = "Error writing config file ";
PRIVATE const char GETIFADDRS[] = "Daemon halted! getifaddrs() failed ";
PRIVATE const char SOCKERR[] = "Error on listening socket ";
PRIVATE const char SYSFAIL1[] = "Syscall fail. Conf file may be ignored ";
PRIVATE const char SYSFAIL2[] = "Syscall fail. Loggin disable ";
PRIVATE const char FORCEDEXIT[] = "Daemon HALTED! ";
PRIVATE const char CONFLICT[] = "Conflict ";

/* Functions definitions */
PUBLIC void startLog()
{
    /*
     * Open a connection to syslog
     */
        openlog(Ident,Option,Facility);
}

PUBLIC void closeLog()
{
        closelog();
}

PUBLIC void logError(int type, char *logStr)
{
    /*
     * Build a error string regardin the error type
     * Several errors are reported
     * Note: The first 54 bytes are reserved to the
     * predifined error-related string
     */
        int len;
        char logBuffer[BUFFERSZ];

        len = 0;
        if (logStr != NULL)
                len = strlen(logStr);
        if (len > LOGSTRMAXLEN)
                len = LOGSTRMAXLEN - 1;
        memset(logBuffer,0,BUFFERSZ);

        switch (type) {
        case EBADPARAMETER:
                strcpy(logBuffer,BADPARAMETER);
                strncat(logBuffer,"(",len);
                strncat(logBuffer,logStr,len);
                strncat(logBuffer,")",len);
                break;
        case ENONAME:
                strcpy(logBuffer,NONAME);
                break;
        case EBADNAME:
                strcpy(logBuffer,BADNAME);
                strncat(logBuffer,"(",len);
                strncat(logBuffer,logStr,len);
                strncat(logBuffer,")",len);
                break;
        case EREPEATIF:
                strcpy(logBuffer,REPEATIF);
                strncat(logBuffer,"(",len);
                strncat(logBuffer,logStr,len);
                strncat(logBuffer,")",len);
                break;
        case ETXTDEFINED:
                strcpy(logBuffer,TXTDEFINED);
                break;
        case EBADLOGLEVEL:
                strcpy(logBuffer,BADLOGLEVEL);
                strncat(logBuffer,"(",len);
                strncat(logBuffer,logStr,len);
                strncat(logBuffer,")",len);
                break;
        case EBADLOGFACILITY:
                strcpy(logBuffer,BADLOGFACILITY);
                strncat(logBuffer,"(",len);
                strncat(logBuffer,logStr,len);
                strncat(logBuffer,")",len);
                break;
        case ELOGLEVELDEFINED:
                strcpy(logBuffer,LOGLEVELDEFINED);
                strncat(logBuffer,"(",len);
                strncat(logBuffer,logStr,len);
                strncat(logBuffer,")",len);
                break;
        case ELOGFACILITYDEFINED:
                strcpy(logBuffer,LOGFACILITYDEFINED);
                strncat(logBuffer,"(",len);
                strncat(logBuffer,logStr,len);
                strncat(logBuffer,")",len);
                break;
        case ELOGFILE:
                strcpy(logBuffer,LOGFILE);
                strncat(logBuffer,logStr,len);
                break;
        case ECREATECONFFILE:
                strcpy(logBuffer,CREATECONFFILE);
                strncat(logBuffer,"(",len);
                strncat(logBuffer,logStr,len);
                strncat(logBuffer,")",len);
                break;
        case ECREATEPIDFILE:
                strcpy(logBuffer,CREATEPIDFILE);
                strncat(logBuffer,"(",len);
                strncat(logBuffer,logStr,len);
                strncat(logBuffer,")",len);
                break;
        case EWRITECONFFILE:
                strcpy(logBuffer,WRITECONFIGFILE);
                strncat(logBuffer,"(",len);
                strncat(logBuffer,logStr,len);
                strncat(logBuffer,")",len);
                break;
        case EGETIFADDRS:
                strcpy(logBuffer,GETIFADDRS);
                strncat(logBuffer,"(",len);
                strncat(logBuffer,logStr,len);
                strncat(logBuffer,")",len);
                break;
        case ESOCKERR:
                strcpy(logBuffer,SOCKERR);
                strncat(logBuffer,"(",len);
                strncat(logBuffer,logStr,len);
                strncat(logBuffer,")",len);
                break;
        case ESYSFAIL1:
                strcpy(logBuffer,SYSFAIL1);
                strncat(logBuffer,"(",len);
                strncat(logBuffer,logStr,len);
                strncat(logBuffer,")",len);
                break;
        case ESYSFAIL2:
                strcpy(logBuffer,SYSFAIL2);
                strncat(logBuffer,"(",len);
                strncat(logBuffer,logStr,len);
                strncat(logBuffer,")",len);
                break;
        case FORCED_EXIT:
                strcpy(logBuffer,FORCEDEXIT);
                break;
        }
        _syslog(logBuffer);
}

PUBLIC void syslogConflict(char *str)
{
        char logBuffer[BUFFERSZ];

        strcpy(logBuffer,CONFLICT);
        strcat(logBuffer,str);
        syslog(LOG_DAEMON | LOG_WARNING,logBuffer);
}

PUBLIC void setLevel(int level)
{
    /*
     * Set syslog level
     */
        Level = level;
}

PUBLIC void setFacility(int facility)
{
    /*
     * Set syslog facility
     */
        Facility = facility;
}

PUBLIC int getSyslogFacility(char *facility)
{
    /*
     * "brute-force" way to convert a string syslog
     * facility to his respective int
     */
        if (!strcasecmp(facility,"LOG_AUTH"))
                return LOG_AUTH;
        else if (!strcasecmp(facility,"LOG_AUTHPRIV"))
                return LOG_AUTHPRIV;
        else if (!strcasecmp(facility,"LOG_CRON"))
                return LOG_CRON;
        else if (!strcasecmp(facility,"LOG_DAEMON"))
                return LOG_DAEMON;
        else if (!strcasecmp(facility,"LOG_FTP"))
                return LOG_FTP;
        else if (!strcasecmp(facility,"LOG_KERN"))
                return LOG_KERN;
        else if (!strcasecmp(facility,"LOG_LPR"))
                return LOG_LPR;
        else if (!strcasecmp(facility,"LOG_MAIL"))
                return LOG_MAIL;
        else if (!strcasecmp(facility,"LOG_NEWS"))
                return LOG_NEWS;
        else if (!strcasecmp(facility,"LOG_SYSLOG"))
                return LOG_SYSLOG;
        else if (!strcasecmp(facility,"LOG_USER"))
                return LOG_USER;
        else if (!strcasecmp(facility,"LOG_UUCP"))
                return LOG_UUCP;
        else if (!strcasecmp(facility,"LOG_LOCAL0"))
                return LOG_LOCAL0;
        else if (!strcasecmp(facility,"LOG_LOCAL1"))
                return LOG_LOCAL1;
        else if (!strcasecmp(facility,"LOG_LOCAL2"))
                return LOG_LOCAL2;
        else if (!strcasecmp(facility,"LOG_LOCAL3"))
                return LOG_LOCAL3;
        else if (!strcasecmp(facility,"LOG_LOCAL4"))
                return LOG_LOCAL4;
        else if (!strcasecmp(facility,"LOG_LOCAL5"))
                return LOG_LOCAL5;
        else if (!strcasecmp(facility,"LOG_LOCAL6"))
                return LOG_LOCAL6;
        else if (!strcasecmp(facility,"LOG_LOCAL7"))
                return LOG_LOCAL7;
        return FAILURE;
}

PUBLIC int getSyslogLevel(char *level)
{
    /*
     * "brute-force" way to convert a string syslog
     * level to his respective int
     */
        if (!strcasecmp(level,"LOG_EMERG"))
                return LOG_EMERG;
        else if (!strcasecmp(level,"LOG_ALERT"))
                return LOG_ALERT;
        else if (!strcasecmp(level,"LOG_CRIT"))
                return LOG_CRIT;
        else if (!strcasecmp(level,"LOG_ERR"))
                return LOG_ERR;
        else if (!strcasecmp(level,"LOG_WARNING"))
                return LOG_WARNING;
        else if (!strcasecmp(level,"LOG_NOTICE"))
                return LOG_NOTICE;
        else if (!strcasecmp(level,"LOG_INFO"))
                return LOG_INFO;
        else if (!strcasecmp(level,"LOG_DEBUG"))
                return LOG_DEBUG;
        return FAILURE;
}
PUBLIC void printSelector()
{
    /*
     * Print the syslog facility and level where to log messages
     */
        printToStream("%d | %d\n",Facility,Level);
}

PRIVATE void _syslog(char *logMsg)
{
    /*
     * Send to syslog
     */
        printToStream("llmnrd[4795]: %s\n",logMsg);
        //syslog(Facility | Level ,logMsg);
}
