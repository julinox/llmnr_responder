/* Macros */
#define LINESZ 300
#define LABELSZ 63
#define MAXLINES 40

/* Includes */
#include <time.h>
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <netinet/in.h>

/* Own includes */
#include "../include/llmnr_defs.h"
#include "../include/llmnr_names.h"
#include "../include/llmnr_net_interface.h"
#include "../include/llmnr_rr.h"
#include "../include/llmnr_conflict_list.h"
#include "../include/llmnr_syslog.h"
#include "../include/llmnr_utils.h"
#include "../include/llmnr_str_list.h"
#include "../include/llmnr_responder_s1.h"

/* Enums & Structs */

/* Private prototypes */
PRIVATE void start();
PRIVATE void getSysName();
PRIVATE void fillSoaRR();
PRIVATE void createConfigFile(char *filePath);
PRIVATE void remLoopback(NETIFACE *ifaces);
PRIVATE void createInterfaces(struct ifaddrs *head);
PRIVATE void fillInterfaces(struct ifaddrs *head);
PRIVATE void _fillInterfaces(NETIFACE *copy, struct ifaddrs *aux);
PRIVATE void fillARecord();
PRIVATE void fillAAAARecord();
PRIVATE void saveLogPath(char *name);
PRIVATE void createPidFile(char *filePath);
PRIVATE void freeResources();
PRIVATE int readConfigFile(char *filePath);
PRIVATE int parseConfigFile(STRNODE *params);
PRIVATE int _parseConfigFile(STRNODE *param);
PRIVATE int validName(char *name);
PRIVATE int validTxt(STRNODE *line);
PRIVATE int validLogLevel(char *level);
PRIVATE int validLogFacility(char *facility);
PRIVATE int validLogConflicts(char *name);
PRIVATE int validLogConflictsOn(char *name);
PRIVATE int validMx(char *pref, char *exchange);
PRIVATE int validIface(char *ifName, char *ifProto);
PRIVATE int checkFQDN(char *name);
PRIVATE int checkDigits(char *str);

/* Glocal variables */

PRIVATE int Errno;
PRIVATE char LogOn;
PRIVATE NAME *Names;
PRIVATE NETIFACE *Ifaces;
PRIVATE RRLIST *Rlist;
PRIVATE CONFLICT *Conflicts;


/* Functions definitions */
PUBLIC void startS1(NAME *_N, NETIFACE *_I, RRLIST *_R, CONFLICT *_C)
{
    /*
     * Set Glocal variables
     */
        Errno = -1;
        LogOn = 1;
        Names = _N;
        Ifaces = _I;
        Rlist = _R;
        Conflicts = _C;
        start();
}

PRIVATE void start()
{
    /*
     * - Check if config file exists. If not create it
     * - If exists then read it and parse it to get config parameters
     * - Call getifaddrs() to get interfaces and ip interfaces
     * - Fill or build interfaces ips, 'A', 'AAAA', 'PTR'
     *   and 'SOA' records
     */
        //int res;
        struct ifaddrs *head;
        char filePath[] = "/etc/llmnr/llmnr.conf";
        char pidFilePath[] = "/etc/llmnr/llmnr.pid";
        char defLogOnPath[] = "syslog";

        if (!access(filePath,F_OK)) {
                if (readConfigFile(filePath) == SYSFAILURE)
                        logError(ESYSFAIL1,strerror(Errno));
        } else {
                createConfigFile(filePath);
        }
        if (nameListSz(Names) <= 1)
                getSysName();
        if (getifaddrs(&head) < 0) {
                Errno = errno;
                logError(EGETIFADDRS,strerror(Errno));
                if (head != NULL)
                        freeifaddrs(head);
                freeResources();
        }
        if (netIfaceListSz(Ifaces) <= 1) {
                createInterfaces(head);
                Ifaces->flags |= _IFF_DYNAMIC;
        } else {
                Ifaces->flags |= _IFF_STATIC;
        }
        if (Conflicts->logPath == NULL && LogOn)
                validLogConflictsOn(defLogOnPath);
        fillPtrRecord(Names);
        fillInterfaces(head);
        freeifaddrs(head);
        fillARecord();
        fillAAAARecord();
        remLoopback(Ifaces);
        fillSoaRR();
        createPidFile(pidFilePath);
}

PRIVATE void createConfigFile(char *filePath)
{
    /*
     * Creates an example config file. If error on creating
     * or writing then report it.
     */
        FILE *file;

        char txt[] =
        "# Example config file for LLMNR (RFC 4795) daemon\n"
        "# This LLMNR daemon is free software; you can redistribute it\n"
        "# and/or modify it under the terms of the GNU Lesser General\n"
        "# Public License as published by the Free Software Foundation.\n"
        "#\n"
        "# hostname config. A machine can have several hostnames:\n"
        "# hostname machine-name1\n"
        "# hostname machine-name2\n"
        "#\n"
        "# Interfaces that will listen/send LLMNR petitions:\n"
        "# iface eth0 ipv4\n"
        "# iface eth0 ipv6\n"
        "# iface wlan0 dual\n"
        "#\n"
        "# syslog config for daemon errors. Default is 'LOG_DAEMON' and 'LOG_ERR':\n"
        "# Log_facility LOG_DAEMON\n"
        "# Log_level LOG_ERR\n"
        "#\n"
        "# Log LLMNR conflicts. Default is LOG_DAEMON | LOG_WARNING via 'syslog':\n"
        "# log_conflicts yes\n"
        "# log_conflicts_on /var/log/llmnr.conflicts\n"
        "#\n"
        "# Resource records config. Only 'MX' and 'TXT' supported:\n"
        "# MX 10 mx.com.ve\n"
        "# TXT some text\n"
        "#\n"
        "# 'CNAME' and 'NS' make no much sense to have in LLMNR\n"
        "# 'A', 'AAAA', 'PTR' and 'SOA' are automatically derivated\n";

        file = fopen(filePath,"w");
        if (file == NULL) {
                Errno = errno;
                logError(ECREATECONFFILE,strerror(Errno));
                return;
        }
        if (fputs(txt,file) < 0) {
                Errno = errno;
                logError(EWRITECONFFILE,strerror(Errno));
        }
        if (file != NULL)
                fclose(file);
}

PRIVATE int readConfigFile(char *filePath)
{
    /*
     * Get parameters from /etc/llmnr.conf and stores them
     * into a 'strList' list for later parsing
     */
        STRNODE *params;
        int i, fd, lineCount;
        char readChar, commentary, line[LINESZ];

        i = 0;
        lineCount = 0;
        commentary = 0;
        memset(line,0,LINESZ + 1);
        fd = open(filePath,O_RDONLY);

        if (fd < 0) {
                Errno = errno;
                return SYSFAILURE;
        }
        params = strNodeHead();
        while (read(fd,&readChar,sizeof(char)) > 0 && lineCount < MAXLINES) {
                if (readChar == '#')
                        commentary = 1;
                if (readChar == 10) {
                        if (i > 0) {
                                if (newStrNode(line,params)) {
                                        Errno = errno;
                                        deleteStrNodeList(&params);
                                        return SYSFAILURE;
                                }
                                lineCount++;
                        }
                        i = 0;
                        commentary = 0;
                        memset(line,0,LINESZ + 1);
                        continue;
                }
                if (!commentary && i < LINESZ)
                        line[i++] = readChar;
        }
        close(fd);
        i = parseConfigFile(params);
        deleteStrNodeList(&params);
        return i;
}

PRIVATE int parseConfigFile(STRNODE *params)
{
    /*
     * Parse the parameters took from config file
     */
        STRNODE *aux;
        int delimiter, res;

        delimiter = 32;
        for (; params != NULL; params = params->next) {
                if (params->string == NULL)
                        continue;
                aux = split(params->string,delimiter);
                if (aux == NULL) {
                        Errno = errno;
                        deleteStrNodeList(&params);
                        return SYSFAILURE;
                }
                if ((res = _parseConfigFile(aux)))
                        logError(res,params->string);
                deleteStrNodeList(&aux);
        }
        return SUCCESS;
}

PRIVATE int _parseConfigFile(STRNODE *param)
{
    /*
     * parseConfigFile() helper
     * Given a string (containing a line read from config file)
     * checks for predefined patterns (hostname, iface, mx,etc)
     * If no pattern found then line its considered an invalid
     * parameter
     * If pattern found then check if the parameter is valid
     */
        char *key;
        int strNodeCount;

        key = getNodeAt(param,1);
        strNodeCount = countStrNodes(param);

        if (strNodeCount == 3 && !strcasecmp("hostname",key))
                return validName(getNodeAt(param,2));
        else if (strNodeCount == 4 && !strcasecmp("iface",key))
                return validIface(getNodeAt(param,2),getNodeAt(param,3));
        else if (strNodeCount == 4 && !strcasecmp("mx",key))
                return validMx(getNodeAt(param,2),getNodeAt(param,3));
        else if (strNodeCount >= 3 && !strcasecmp("txt",key))
                return validTxt(param);
        else if (strNodeCount == 3 && !strcasecmp("log_facility",key))
                return validLogFacility(getNodeAt(param,2));
        else if (strNodeCount == 3 && !strcasecmp("log_level",key))
                return validLogLevel(getNodeAt(param,2));
        else if (strNodeCount == 3 && !strcasecmp("log_conflicts",key))
                return validLogConflicts(getNodeAt(param,2));
        else if (strNodeCount == 3 && !strcasecmp("log_conflicts_on",key))
                return validLogConflictsOn(getNodeAt(param,2));
        else
                return EBADPARAMETER;
        return SUCCESS;
}

PRIVATE int validName(char *name)
{
    /*
     * Checks if the given hostname (read from config
     * file) is valid. If valid then a new 'NAME' node
     * is created
     */
        char buff[HOSTNAMEMAX + 1];

        if (checkFQDN(name)) {
                return EBADNAME;
        } else {
                strToDnsStr(name,buff);
                newName(buff,Names);
        }
        return SUCCESS;
}

PRIVATE int validIface(char *ifName, char *family)
{
    /*
     * Checks if the given interface (read from config
     * file is valid. If valid then a new 'NETIFACE' node
     * is created
     */
        if (!strcasecmp(family,"ipv4")) {
                if (addNetIfNode(ifName,AF_INET,_IFF_STATIC,Ifaces) == FAILURE)
                        return EREPEATIF;
        }
        else if (!strcasecmp(family,"ipv6")) {
                if (addNetIfNode(ifName,AF_INET6,_IFF_STATIC,Ifaces) == FAILURE)
                        return EREPEATIF;
        }
        else if (!strcasecmp(family,"dual")) {
                if (addNetIfNode(ifName,AF_DUAL,_IFF_STATIC,Ifaces))
                        return EREPEATIF;
        }
        else
                return EBADPARAMETER;
        return SUCCESS;
}

PRIVATE int validMx(char *pref, char *exchange)
{
    /*
     * Checks if the given 'MX' (read from config file
     * is valid. If valid then a new 'RRLIST' node
     * is created
     */
        MX_RR mxrr;
        RESRECORD rr;
        short preference;
        char buffer[HOSTNAMEMAX + 1];

        if (checkDigits(pref))
                return EBADPARAMETER;
        preference = atoi(pref);
        if (checkFQDN(exchange))
                return EBADNAME;
        memset(buffer,0,sizeof(buffer));
        strToDnsStr(exchange,buffer);
        mxrr.PREFERENCE = preference;
        mxrr.EXCHANGE = buffer;
        rr.TYPE = MX;
        rr.CLASS = INCLASS;
        rr.TTL = RRTTL;
        rr.RDLENGTH = sizeof(short) + strlen(buffer) + 1;
        rr.RDATA = (void *)&mxrr;
        newResRecord(Rlist,&rr);
        return SUCCESS;
}

PRIVATE int validTxt(STRNODE *line)
{
    /*
     * Checks if the given 'TXT' (read from config file
     * is valid. If valid then a new 'RRLIST' node
     * is created
     */
        TXT_RR txtrr;
        RESRECORD rr;
        int i, lineC;
        static char txtDef = 0;
        char fBuff[LINESZ], *buff;

        if (txtDef > 0)
                return ETXTDEFINED;
        memset(fBuff,0,LINESZ);
        lineC = countStrNodes(line);

        buff = fBuff + 1;
        for (i = 2; i < lineC; i++) {
                strcat(buff,getNodeAt(line,i));
                if (i < lineC - 1)
                        strcat(buff," ");
        }
        *fBuff = strlen(buff);
        txtrr.TXTDATA = fBuff;
        rr.TYPE = TXT;
        rr.CLASS = INCLASS;
        rr.TTL = RRTTL;
        rr.RDLENGTH = strlen(fBuff);
        rr.RDATA = (void *)&txtrr;
        newResRecord(Rlist,&rr);
        return SUCCESS;
}

PRIVATE int validLogFacility(char *facility)
{
    /*
     * Checks if the given log faciliy (read from config
     * file is valid. If valid then the log facility is
     * setted
     */
        int dFacility;
        static int defined = 0;

        if (defined)
                return ELOGFACILITYDEFINED;
        dFacility = getSyslogFacility(facility);
        if (dFacility >= 0) {
                setFacility(dFacility);
                defined = 1;
        } else {
                return EBADLOGFACILITY;
        }
        return SUCCESS;
}

PRIVATE int validLogLevel(char *level)
{
    /*
     * Checks if the given log level (read from config
     * file is valid. If valid then the log level is
     * setted
     */
        int dLevel;
        static int defined = 0;

        if (defined)
                return ELOGLEVELDEFINED;
        dLevel = getSyslogLevel(level);
        if (dLevel >= 0) {
                setLevel(dLevel);
                defined = 1;
        } else {
                return EBADLOGLEVEL;
        }
        return SUCCESS;
}

PRIVATE int validLogConflicts(char *name)
{
    /*
     * Checks if LLMNR conflicts must be saved
     */
        if (!strcasecmp("yes",name))
                LogOn = 1;
        else if (!strcasecmp("no",name))
                LogOn = 0;
        else
                return EBADPARAMETER;
        return SUCCESS;
}

PRIVATE int validLogConflictsOn(char *name)
{
    /*
     * Checks if given name its a correct file path where
     * to store LLMNR conflicts
     */
        FILE *file;
        U_CHAR save;

        save = 0;
        file = NULL;
        if (!strcasecmp(name,"syslog")) {
                save = 1;
        } else {
                if (access(name,F_OK | W_OK)) {
                        file = fopen(name,"w");
                        if (file == NULL)
                                logError(ELOGFILE,name);
                        else
                                save = 1;
                } else {
                        save = 1;
                }
        }
        if (file != NULL) {
                fclose(file);
                remove(name);
        }
        if (save)
                saveLogPath(name);
        return SUCCESS;
}

PRIVATE int checkFQDN(char *name)
{
    /*
     * Checks if a given hostaname is a valid FQDN
     */
        int i, len;
        STRNODE *sp, *aux;

        if (name == NULL)
                return SUCCESS;
        len = strlen(name);
        if (len > HOSTNAMEMAX)
                return FAILURE;
        for (i = 0; i < len; i++) {
                if (!isalnum(name[i]) && name[i] != '-'
                    && name[i] != '.')
                return FAILURE;        }

        if (name[0] == '-' || name[len - 1] == '-')
                return FAILURE;
        if (name[0] == '.' && len > 1)
                return FAILURE;
        for (i = 0; i < len; i++) {
                if (name[i] == '.' || name[i] == '-') {
                        if (name[i + 1] == '.' || name[i + 1] == '-')
                                return FAILURE;
                }
        }
        sp = split(name,'.');
        aux = sp;
        while (aux != NULL) {
                if (aux->string != NULL) {
                        if (strlen(aux->string) > LABELSZ)
                                return FAILURE;
                }
                aux = aux->next;
        }
        deleteStrNodeList(&sp);
        return SUCCESS;
}

PRIVATE int checkDigits(char *str)
{
    /*
     * Checks if a given string contains a valid
     * integer
     */
        size_t i;

        i = 0;
        for (; i < strlen(str); i++) {
                if (!isdigit(str[i]))
                        return FAILURE;
        }
        return SUCCESS;
}

PRIVATE void createInterfaces(struct ifaddrs *head)
{
    /*
     * Dinamic interfaces. No interface were specify in
     * config file. Add to 'NETIFACE' list every network
     * interface available in the system
     */
        int family;
        struct ifaddrs *current;

        if (head == NULL)
                return;
        current = head;
        for (; current != NULL; current = current->ifa_next) {
                if (current->ifa_addr == NULL)
                        continue;
                family = current->ifa_addr->sa_family;
                if (current->ifa_flags & IFF_LOOPBACK)
                        continue;
                if (family == AF_INET)
                        addNetIfNode(current->ifa_name,AF_INET,_IFF_DYNAMIC,
                                     Ifaces);
                else if (family == AF_INET6)
                        addNetIfNode(current->ifa_name,AF_INET6,_IFF_DYNAMIC,
                                     Ifaces);
        }
}

PRIVATE void fillInterfaces(struct ifaddrs *head)
{
    /*
     * For every interface in the 'NETIFACE' list add
     * their corresponding ips (Both IPv4 & IPv6)
     */
        NETIFACE *copy;
        int index, family;
        struct ifaddrs *aux;

        copy = Ifaces;
        for (; copy != NULL; copy = copy->next) {
                for (aux = head; aux != NULL; aux = aux->ifa_next) {
                        index = if_nametoindex(aux->ifa_name);
                        if (index == copy->ifIndex) {
                                if (aux->ifa_addr == NULL)
                                        continue;
                                family = aux->ifa_addr->sa_family;
                                if (family != AF_INET && family != AF_INET6)
                                        continue;
                                if (aux->ifa_flags & IFF_LOOPBACK) {
                                        copy->flags |= _IFF_LOOPBACK;
                                        continue;
                                }
                                _fillInterfaces(copy,aux);
                        }
                }
        }
}

PRIVATE void _fillInterfaces(NETIFACE *copy, struct ifaddrs *aux)
{
    /*
     * Helper function for fillInterfaces()
     */
        INADDR *ip4;
        IN6ADDR *ip6;

        switch (aux->ifa_addr->sa_family) {

        case AF_INET:
                ip4 = (INADDR *)(&((SA_IN *)aux->ifa_addr)->sin_addr);
                addNetIfIPv4(ip4,copy);
                if (!(aux->ifa_flags & IFF_RUNNING))
                        copy->flags &= ~_IFF_RUNNING;
                break;
        case AF_INET6:
                ip6 = (IN6ADDR *)(&((SA_IN6 *)aux->ifa_addr)->sin6_addr);
                addNetIfIPv6(ip6,copy);
                if (!(aux->ifa_flags & IFF_RUNNING))
                        copy->flags &= ~_IFF_RUNNING;
                break;
        }
}

PRIVATE void remLoopback(NETIFACE *ifaces)
{
    /*
     * Remove all loopback interfaces (if any) from the
     * 'NETIFACE' list
     */
        NETIFACE *current;

        if (ifaces == NULL)
                return;
        current = ifaces;
        while (current != NULL) {
                if (current->flags & _IFF_LOOPBACK) {
                        remNetIfNode(current->name,ifaces);
                        break;
                }
                current = current->next;
        }
}

PRIVATE void fillARecord()
{
    /*
     * For every IPv4 of every interface "build" his
     * corresponding 'A' record (See llmnr_rr.h)
     */
        NETIFACE *current;
        NETIFIPV4 *ipv4Ptr;

        current = Ifaces;
        for (; current != NULL; current = current->next)
        {
                if (current->name == NULL)
                        continue;
                ipv4Ptr = current->IPv4s;
                for (; ipv4Ptr != NULL; ipv4Ptr = ipv4Ptr->next) {
                        if (ipv4Ptr->ipType == NOIP)
                                continue;
                        buildARecord(&ipv4Ptr->ip4Addr,ipv4Ptr->ARECORD,
                                     ipv4Ptr->PTR4RECORD);
                }
        }
}

PRIVATE void fillAAAARecord()
{
    /*
     * Same thing that fillARecord() but with IPv6
     */

    NETIFACE *current;
    NETIFIPV6 *ipv6Ptr;

    current = Ifaces;
    for (; current != NULL; current = current->next) {
            if (current->name == NULL)
                    continue;
            ipv6Ptr = current->IPv6s;
            for (; ipv6Ptr != NULL; ipv6Ptr = ipv6Ptr->next) {
                    if (ipv6Ptr->ipType == NOIP)
                            continue;
                    buildAAAARecord(&ipv6Ptr->ip6Addr,ipv6Ptr->AAAARECORD,
                                    ipv6Ptr->PTR6RECORD);
            }
    }
}

PRIVATE void fillSoaRR()
{
    /*
     * Create 'SOA' record and store it into 'RRLIST' list
     */
        char aux[2];
        SOA_RR soarr;
        RESRECORD rr;

        *aux = 0xC0;
        *(aux + 1) = HEADSZ;

        rr.TYPE = SOA;
        rr.CLASS = INCLASS;
        rr.TTL = RRTTL;

        soarr.MNAME = aux;
        soarr.RNAME = aux;
        soarr.SERIAL = 0;
        soarr.REFRESH = 86400;
        soarr.RETRY = 7200;
        soarr.EXPIRE = 3600000;
        soarr.MINIMUN = 1440;
        rr.RDLENGTH = (2 * sizeof(unsigned short)) +
                      (5 * sizeof (unsigned int));
        rr.RDATA = (void *)&soarr;
        newResRecord(Rlist,&rr);
}

PRIVATE void getSysName()
{
    /*
     * If ho hostaname specified in config file then read it
     * from the system.
     * Reads the domain name because a machine is also
     * authoritative of his FQDN (hostname + domain name. See
     * RFC 4795)
     */
        int len;
        char buffer[HOSTNAMEMAX + 1];
        char doName[HOSTNAMEMAX + 1];
        char domain[HOSTNAMEMAX + 1];
        char hostName[HOSTNAMEMAX + 1];

        memset(domain,0,sizeof(domain));
        memset(hostName,0,sizeof(hostName));
        getdomainname(domain,sizeof(domain));
        gethostname(hostName,sizeof(hostName));

        if (strlen(hostName) == 0 || checkFQDN(hostName)) {
                logError(ENONAME,NULL);
                freeResources();
        }
        memset(buffer,0,HOSTNAMEMAX + 1);
        strToDnsStr(hostName,buffer);
        newName(buffer,Names);

        if (strlen(domain) > 0 && !checkFQDN(domain)) {
                len = strlen(hostName) + strlen(domain) + 3;
                if (len <= HOSTNAMEMAX) {
                        strcpy(doName,hostName);
                        if (hostName[strlen(hostName) - 1] != '.')
                                doName[strlen(doName)] = '.';
                        strcat(doName,domain);
                        memset(buffer,0,HOSTNAMEMAX + 1);
                        strToDnsStr(doName,buffer);
                        newName(buffer,Names);
                }
        }
}

PRIVATE void saveLogPath(char *name)
{
    /*
     * Creates and store the path file where
     * to store LLMNR conflicts
     */
        if (LogOn == 0)
                return;
        Conflicts->logPath = calloc(1,strlen(name) + 1);
        if (Conflicts->logPath == NULL) {
                Errno = errno;
                logError(ESYSFAIL2,strerror(Errno));
                return;
        }
        strcpy(Conflicts->logPath,name);
}

PRIVATE void createPidFile(char *filePath)
{
        FILE *file;

        file = fopen(filePath,"w");
        if (file == NULL) {
                Errno = errno;
                logError(ECREATEPIDFILE,strerror(Errno));
                return;
        }
        fprintf(file,"%d\n",getpid());
        fclose(file);
}

PRIVATE void freeResources()
{
    /*
     * Free resources in case of fatal error
     */
        if (Names != NULL)
                deleteNameList(&Names);
        if (Ifaces != NULL)
                delNetIfList(&Ifaces);
        if (Rlist != NULL)
                deleteRList(&Rlist);
        if (Conflicts != NULL)
                deleteConflictList(&Conflicts);
        exit(EXIT_FAILURE);
}
