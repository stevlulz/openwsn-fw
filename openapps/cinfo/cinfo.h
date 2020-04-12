#ifndef __CINFO_H
#define __CINFO_H

/**
\addtogroup AppCoAP
\{
\addtogroup cinfo
\{
*/

#include "opencoap.h"

//=========================== define ==========================================

//=========================== typedef =========================================

//=========================== variables =======================================

typedef struct {
   coap_resource_desc_t desc;
   opentimers_id_t              timerId;
   int fd;

   int count;
   int start;
} cinfo_vars_t;

//=========================== prototypes ======================================

void cinfo_init(void);

/**
\}
\}
*/

#endif
