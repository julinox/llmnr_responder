/* Macros */

/* Includes */
#include <poll.h>
#include <time.h>
#include <unistd.h>
#include <netinet/in.h>

/* Own includes */
#include "../include/llmnr_defs.h"
#include "../include/llmnr_names.h"
#include "../include/llmnr_net_interface.h"
#include "../include/llmnr_rr.h"
#include "../include/llmnr_conflict_list.h"
#include "../include/llmnr_syslog.h"
#include "../include/llmnr_print.h"
#include "../include/llmnr_responder_s1.h"
#include "../include/llmnr_responder_s2.h"

/* Enums & Structs */

/* Private prototypes */

/* Glocal variables */

/* Functions definitions */
PUBLIC int main(void)
{
        NAME *names;
        NETIFACE *ifaces;
        RRLIST *rList;
        CONFLICT *conflicts;

        daemon(0,0);
        setStream("~/salida.txt");
        startLog();
        names = nameListHead();
        ifaces = NetIfListHead();
        rList = rrListHead();
        conflicts = conflictListHead();
        startS1(names,ifaces,rList,conflicts);
        startS2(names,ifaces,rList,conflicts);
        return SUCCESS;
}
