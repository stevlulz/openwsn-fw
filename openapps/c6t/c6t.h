/**
\brief CoAP 6top application

\author Xavi Vilajosana <xvilajosana@eecs.berkeley.edu>, February 2013.
\author Thomas Watteyne <watteyne@eecs.berkeley.edu>, July 2014
*/

#ifndef __C6T_H
#define __C6T_H

/**
\addtogroup AppCoAP
\{
\addtogroup c6t
\{
*/

#include "opendefs.h"
#include "opencoap.h"
#include "schedule.h"

//=========================== define ==========================================

//=========================== typedef =========================================

//=========================== variables =======================================

typedef struct {
   coap_resource_desc_t desc;
   cellInfo_ht          celllist_delete[CELLLIST_MAX_LEN];
   int valid;
   int delete_count;
   int                  fd;

   int done;
} c6t_vars_t;

//=========================== prototypes ======================================

void c6t_init(void);

/**
\}
\}
*/

#endif
