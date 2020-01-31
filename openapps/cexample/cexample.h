#ifndef __CEXAMPLE_H
#define __CEXAMPLE_H

/**
\addtogroup AppUdp
\{
\addtogroup cexample
\{
*/

#include "opencoap.h"
#include "opendefs.h"

//=========================== define ==========================================
#define GRAPH_SIZE 10
//=========================== typedef =========================================
typedef struct {
  uint8_t rssi; // idmanager_getMyID(ADDR_16B);
  uint8_t is_parent : 1;
  uint8_t link : 1;

}graph_entry_t;

typedef struct {
   coap_resource_desc_t         desc;


   graph_entry_t v[GRAPH_SIZE+1][GRAPH_SIZE+1];

   int fd;
} cexample_vars_t;

//=========================== variables =======================================

//=========================== prototypes ======================================

void cexample_init(void);


/**
\}
\}
*/

#endif
