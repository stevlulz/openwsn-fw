#ifndef __CEXAMPLE_H
#define __CEXAMPLE_H

/**
\addtogroup AppUdp
\{
\addtogroup cexample
\{
*/
#include "opendefs.h"

#include "opencoap.h"

//=========================== define ==========================================
#define GRAPH_SIZE 10
#define ADD_NEIGHBOR 32
#define ADD_NEIGHBORS 33
#define DEL_NEIGHBOR 34
#define DEL_NEIGHBORS 35
#define CHANGE_PARENT 36
#define DEFINE_PARENT 37
#define UPDATE_NEIGHBOR 38
#define UPDATE_NEIGHBORS 39





//=========================== typedef =========================================
typedef struct {
  int8_t rssi; // idmanager_getMyID(ADDR_16B);
  uint8_t is_parent : 1;
  uint8_t link : 1;

}graph_entry_t;

typedef struct {
   coap_resource_desc_t         desc;
   opentimers_id_t              timerId;


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
