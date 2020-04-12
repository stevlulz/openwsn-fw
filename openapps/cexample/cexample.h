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
  uint8_t l_from;
  uint8_t r_from;
  uint8_t l_to;
  uint8_t r_to;
  int8_t  rssi;

}link_anounce;

typedef struct {
  uint8_t l_to;
  uint8_t r_to;
   int8_t  rssi;
}link_announce_t;

typedef struct {
  uint8_t code;
  uint8_t flags : 7; //expand this later
  uint8_t parent_exist : 1;
  uint8_t neighbors_count;
  uint8_t m_left;
  uint8_t m_right;
  uint8_t p_left;
  uint8_t p_right;
}links_update_t;


typedef struct {
   coap_resource_desc_t         desc;
   opentimers_id_t              timerId;



   uint8_t          old : 1;
   uint8_t          join : 1;

   int fd;
} cexample_vars_t;

//=========================== variables =======================================

//=========================== prototypes ======================================

void cexample_init(void);
void cexample_sendaddnewneighbors(void);
void cexample_sendaddnewneighbor(uint8_t l_from,uint8_t r_from,uint8_t l_to,uint8_t r_to,int8_t rssi);


/**
\}
\}
*/

#endif
