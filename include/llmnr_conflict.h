/** **********************************************************
 * cDar = Conflict detection and resolution                  *
 * On a given Name and a given interface this "micro module" *
 * will perform, as described in RFC 4795, a conflict        *
 * resolution. Also will detect if two or more interfaces    *
 * are operating in the same network                         *
 * Note: Previous mutex sincronization is performed before   *
 * calling this "micro module"                               *
 *************************************************************/

#ifndef LLMNR_CONFLICT_H
#define LLMNR_CONFLICT_H

PUBLIC void cDar(NAME *name, NETIFACE *iface, NETIFACE *ifs);

#endif
