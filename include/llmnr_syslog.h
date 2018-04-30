/** *******************************************
 *  Interface to report errors to syslog      *
 *  Default selector for errors is LOG_DAEMON *
 *  and LOG_ERR                               *
 **********************************************/

#ifndef LLMNR_SYSLOG_H
#define LLMNR_SYSLOG_H

enum LLMNRERROR {

        EBADPARAMETER = 1,
        ENONAME,
        EBADNAME,
        EREPEATIF,
        ETXTDEFINED,
        EBADLOGLEVEL,
        EBADLOGFACILITY,
        ELOGLEVELDEFINED,
        ELOGFACILITYDEFINED,
        ELOGFILE,
        ENOLOGGING,
        ECREATECONFFILE,
        ECREATEPIDFILE,
        EWRITECONFFILE,
        EGETIFADDRS,
        EPOLLERR,
        ESOCKERR,
        ESYSFAIL1,
        ESYSFAIL2,
        FORCED_EXIT,
        LOGCONFLICT
};

PUBLIC void startLog();
PUBLIC void closeLog();
PUBLIC void printSelector();
PUBLIC void setLevel(int level);
PUBLIC void syslogConflict(char *str);
PUBLIC void logError(int type, char *logStr);
PUBLIC void setFacility(int facility);
PUBLIC int getSyslogLevel(char *level);
PUBLIC int getSyslogFacility(char *facility);

#endif
