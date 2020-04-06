/**
\brief An example CoAP application.
*/


/**

  TODO
      - Upon startup all nodes uses MSF.
      - ALL nodes send neighbor information to Orch...
      - Orch construct scheduling table
      - [Orche] TRY to initilize the Scheduling table....
      - [Orche] Start to announce scheduling information starting from nearest node to farest one.
      - [Nodes {SENDER}] Upon receiving scheduling information, update the scheduling [slot,chan] with neighbor+Clear old one
**/
#include "opendefs.h"
#include "cexample.h"
#include "opencoap.h"
#include "openqueue.h"
#include "packetfunctions.h"
#include "openserial.h"
#include "openrandom.h"
#include "scheduler.h"
//#include "ADC_Channel.h"
#include "idmanager.h"
#include "IEEE802154E.h"
#include "schedule.h"
#include "icmpv6rpl.h"
#include "neighbors.h"


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
//=========================== defines =========================================
#define CEXAMPLE_NOTIFY_PERIOD  20000
#define CEXAMPLE_UPDATE_PERIOD  10000

const uint8_t cexample_path0[] = "tasa";

const uint8_t c6Y[] = "6t";

static uint8_t m[16] = {0xbb, 0xbb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
//=========================== variables =======================================

cexample_vars_t cexample_vars;

//=========================== prototypes ======================================

owerror_t cexample_receive( OpenQueueEntry_t* msg,
        coap_header_iht*  coap_header,
        coap_option_iht*  coap_incomingOptions,
        coap_option_iht*  coap_outgoingOptions,
        uint8_t*          coap_outgoingOptionsLen);


void    cexample_sendDone(OpenQueueEntry_t* msg,
                       owerror_t error);
void send6t(void);
void   cexample_init_graph(void);
void   cexample_log_topo(void);
void   cexample_parse(OpenQueueEntry_t* msg);


void   cexample_timer_cb(opentimers_id_t id);
void   cexample_task_cb(void);

void   cexample_timer_cb2(opentimers_id_t id);
void   send_msg(void);
void   request_first_join(void);
void cexample_send_link_update(void);

//=========================== public ==========================================

void cexample_init(void) {
    // check if am centrilized node ??

    open_addr_t     temp_neighbor;
    memset(&temp_neighbor,0,sizeof(temp_neighbor));
    uint16_t        slot = SCHEDULE_MINIMAL_6TISCH_ACTIVE_CELLS+
                           (((((uint16_t)(idmanager_getMyID(ADDR_64B)->addr_64b[6]))<<8) + (uint16_t)(idmanager_getMyID(ADDR_64B)->addr_64b[7])) %
                           (SLOTFRAME_LENGTH-SCHEDULE_MINIMAL_6TISCH_ACTIVE_CELLS));

    uint16_t        channel = (((uint16_t)(idmanager_getMyID(ADDR_64B)->addr_64b[6]))<<8) + (uint16_t)(idmanager_getMyID(ADDR_64B)->addr_64b[7]);

    channel = channel % NUM_CHANNELS;

    temp_neighbor.type             = ADDR_ANYCAST;
    schedule_addActiveSlot(
        slot,     // slot offset
        CELLTYPE_RX,                                                     // type of slot
        FALSE,                                                           // shared?
        TRUE,                                                            // auto cell?
        channel,  // channel offset
        &temp_neighbor                                                   // neighbor
    );




    char file[50];
    memset(file,0,50);
    open_addr_t* addr = idmanager_getMyID(ADDR_16B);

    unsigned int l = ((int)addr->addr_16b[0]);
    unsigned int r = ((int)addr->addr_16b[1]);
    snprintf(file,50,"/home/stevlulz/Desktop/cexample_log.%d.%d.txt",l,r);
    cexample_vars.fd = open(file,O_CREAT | O_RDWR);
    int fd = cexample_vars.fd;
    if(r == 0x1 && l == 0x0){
      dprintf(fd,"[+]{%d.%d} GATEWAY\n",l,r);
    }
    else{
      dprintf(fd,"[+]{%d.%d} NODE\n",l,r);
    }
    cexample_vars.timerId    = opentimers_create(TIMER_GENERAL_PURPOSE, TASKPRIO_COAP);
    cexample_vars.join = 0;
    opentimers_scheduleIn(
        cexample_vars.timerId,
        CEXAMPLE_NOTIFY_PERIOD,
        TIME_MS,
        TIMER_PERIODIC,
        cexample_timer_cb
   );
}

//=========================== private =========================================
int count = 0;

void cexample_init_graph(void){
}
void   cexample_log_topo(void){
}
void   cexample_parse(OpenQueueEntry_t* msg){

}
owerror_t cexample_receive( OpenQueueEntry_t* msg,
        coap_header_iht*  coap_header,
        coap_option_iht*  coap_incomingOptions,
        coap_option_iht*  coap_outgoingOptions,
        uint8_t*          coap_outgoingOptionsLen){

        owerror_t outcome;
        outcome                          = E_SUCCESS;
        int fd = cexample_vars.fd;
        dprintf(fd,"[+] RECEIVED A MESSAGE TOBE DELETED");
        return outcome;

}
void cexample_sendDone(OpenQueueEntry_t* msg, owerror_t error) {
    openqueue_freePacketBuffer(msg);
}

void cexample_timer_cb(opentimers_id_t id){

    int fd = cexample_vars.fd;

    if (ieee154e_isSynch()==FALSE) {
        dprintf(fd,"[-]not synch\n");
        return;
    }
    dprintf(fd,"[-]synch\n");

    request_first_join();

    cexample_task_cb();
}
void cexample_timer_cb2(opentimers_id_t id){

}

void request_first_join(void){
  int fd = cexample_vars.fd;
  open_addr_t    parentNeighbor;
  open_addr_t    nonParentNeighbor;
  cellInfo_ht    celllist_add[CELLLIST_MAX_LEN];
  bool           foundNeighbor;
/*
  foundNeighbor = icmpv6rpl_getPreferredParentEui64(&parentNeighbor);
  if (foundNeighbor==FALSE) {
      dprintf(fd,"--------------------> No parent found\n");
      return;
  }
    dprintf(fd,"--------------------> parent found -- \n");
  dprintf(fd,"----------------------> have parent\n");

  if (schedule_hasNegotiatedTxCellToNonParent(&parentNeighbor, &nonParentNeighbor)==TRUE){

      // send a clear request to the non-parent neighbor
      dprintf(fd,"-----------------> hasNegotiatedTxCellToNonParent \n");

      sixtop_request(
          IANA_6TOP_CMD_CLEAR,     // code
          &nonParentNeighbor,      // neighbor
          NUMCELLS_MSF,            // number cells
          CELLOPTIONS_MSF,         // cellOptions
          NULL,                    // celllist to add (not used)
          NULL,                    // celllist to delete (not used)
          IANA_6TISCH_SFID_MSF,    // sfid
          0,                       // list command offset (not used)
          0                        // list command maximum celllist (not used)
      );
  }

    uint8_t num_ =schedule_getNumberOfNegotiatedCells(&parentNeighbor, CELLTYPE_TX);
      dprintf(fd,"--------------------> Count %d \n",num_);
  if (num_ ==0){

      //if (msf_candidateAddCellList(celllist_add,NUMCELLS_MSF)==FALSE){
          // failed to get cell list to add
     //     dprintf(fd,"[+] failed to get cell list to add \n");

       //   return;
      //}

      //  bool      isUsed;
       // uint16_t  slotoffset;
        //uint16_t  channeloffset;

      uint8_t* from = idmanager_getMyID(ADDR_16B)->addr_16b;
      celllist_add[1].isUsed = FALSE;
      celllist_add[2].isUsed = FALSE;
      celllist_add[3].isUsed = FALSE;
      celllist_add[4].isUsed = FALSE;
      celllist_add[5].isUsed = FALSE;
      celllist_add[0].slotoffset =  (from[1]%101);
      celllist_add[0].channeloffset = from[1]%15 ;
      dprintf(fd,"[+] negotiated with parent id : %d -- slot : %d  -- channeloff : %d \n",
                    from[1],celllist_add[0].slotoffset,celllist_add[0].channeloffset);
      sixtop_request(
          IANA_6TOP_CMD_ADD,           // code
          &parentNeighbor,            // neighbor
          NUMCELLS_MSF,                // number cells
          CELLOPTIONS_TX,                 // cellOptions
          celllist_add,                // celllist to add
          NULL,                        // celllist to delete (not used)
          IANA_6TISCH_SFID_MSF,        // sfid
          0,                           // list command offset (not used)
          0                            // list command maximum celllist (not used)
      );
   }
*/
   cexample_vars.join = 0;

}

void cexample_send_link_update(void){
    //INTERRUPT

    uint8_t num = neighbors_getNumNeighbors();
    int fd = cexample_vars.fd;
    uint8_t *from,*to;
    uint8_t i;
    open_addr_t* tmp = NULL;
    if(num == 0){
      dprintf(fd,"\t[+] I DO NOT HAVE NEIGHBORS \n");
      return;
    }


    if(num == 1){
      for (i = 0; i < 30; i++) {
        if(neighbors_getUsed(i) == TRUE){
          tmp = neighbors_get(i);
          break;
        }
      }
      if(tmp == NULL){
        dprintf(fd,"\t[+] I DID NOT FIND A NEIGHBOR\n");
        return;
      }

      from = idmanager_getMyID(ADDR_16B)->addr_16b;
      to = &tmp->addr_64b[0];
      int8_t rssi = neighbors_getRssi(i);
      dprintf(fd,"\t[+] I HAVE ONE NEIGHBOR \n"
                 "\t\t[+] sent : FROM %02x %02x -- TO %02x %02x -- rssi %d\n",
                 from[0],from[1],to[6],to[7],rssi);
     dprintf(fd,"\n\t\t\t");
     for (size_t i = 0; i < 8; i++) {
        dprintf(fd,"%02x ",tmp->addr_64b[i]);
     }
     dprintf(fd,"\n");

    cexample_sendaddnewneighbor(from[0],from[1],to[6],to[7],rssi);
    return ;
    }


    dprintf(fd,"\t[+] I HAVE MANY NEIGHBORS \n");
    cexample_sendaddnewneighbors();
}


void cexample_task_cb(void){
    int fd = cexample_vars.fd;
    dprintf(fd,"[+] {%d} cexample_task_cb SEND UPDATE\n",count++);
    cexample_send_link_update();
    return;
}

//announce one neighbor
void cexample_sendaddnewneighbor(uint8_t l_from,uint8_t r_from,uint8_t l_to,uint8_t r_to,int8_t rssi){
      int fd = cexample_vars.fd;
      OpenQueueEntry_t* pkt;
      owerror_t outcome;
      coap_option_iht options[1];


      pkt = openqueue_getFreePacketBuffer(COMPONENT_CEXAMPLE);
      if (pkt == NULL) {
          openserial_printError(COMPONENT_CEXAMPLE,ERR_BUSY_SENDING,
                                (errorparameter_t)0,
                                (errorparameter_t)0);
          return;
      }
      dprintf(fd,"[+] GOT FREE PACKET\n");

      pkt->creator   = COMPONENT_CEXAMPLE;
      pkt->owner      = COMPONENT_CEXAMPLE;
      pkt->l4_protocol  = IANA_UDP;

      packetfunctions_reserveHeaderSize(pkt,8);

      pkt->payload[0] = 0x20; // add link code
      pkt->payload[1] = l_from; // my ID {from_node}
      pkt->payload[2] = r_from;

      pkt->payload[3] = l_to; // neighbor ID  {to_node}
      pkt->payload[4] = r_to;

      pkt->payload[5] = rssi;

      pkt->payload[6] = 0xff;
      pkt->payload[7] = 0xff;
      dprintf(fd,"[+] COPIED BUFFER\n");
      // location-path option
      options[0].type = COAP_OPTION_NUM_URIPATH;
      options[0].length = sizeof(cexample_path0) - 1;
      options[0].pValue = (uint8_t *) cexample_path0;
      //metada
      pkt->l4_destination_port   = WKP_UDP_COAP;
      pkt->l3_destinationAdd.type = ADDR_128B;
       // set destination address here

      memcpy(&pkt->l3_destinationAdd.addr_128b[0],&m[0],16);
      dprintf(fd,"[+] INIT DESTINATION\n");

      //send
      outcome = opencoap_send(
              pkt,
              COAP_TYPE_NON,
              COAP_CODE_REQ_PUT,
              1, // token len
              options,
              1, // options len
              &cexample_vars.desc
              );
      if (outcome == E_FAIL) {
        openqueue_freePacketBuffer(pkt);
        dprintf(fd,"[+] PACKET FAILED\n");
        return ;
      }
      dprintf(fd,"[+] PACKET WAS SENT\n");
}

// announce more than one neighbor
void cexample_sendaddnewneighbors(void){
   uint8_t num = neighbors_getNumNeighbors();
   int fd = cexample_vars.fd;
   OpenQueueEntry_t* pkt;
   owerror_t outcome;
   coap_option_iht options[1];
   uint8_t *from;
   uint8_t i;
   pkt = openqueue_getFreePacketBuffer(COMPONENT_CEXAMPLE);
   if (pkt == NULL) {
       openserial_printError(COMPONENT_CEXAMPLE,ERR_BUSY_SENDING,
                             (errorparameter_t)0,
                             (errorparameter_t)0);
       return;
   }
   dprintf(fd,"[+] GOT FREE PACKET\n");
   pkt->payload                     = &(pkt->packet[127]);
   pkt->length                      = 0;

   pkt->creator   = COMPONENT_CEXAMPLE;
   pkt->owner      = COMPONENT_CEXAMPLE;
   pkt->l4_protocol  = IANA_UDP;
   open_addr_t* tmp = NULL;

   from = idmanager_getMyID(ADDR_16B)->addr_16b;
   uint8_t p_index = 0;
   bool parent_exists = icmpv6rpl_getPreferredParentIndex(&p_index);

   packetfunctions_reserveHeaderSize(pkt,1);
   pkt->payload[0] = 0xFF;

   packetfunctions_reserveHeaderSize(pkt,num*sizeof(link_announce_t)+7);
   links_update_t * header = (links_update_t *)&pkt->payload[0];
   link_announce_t *content = (link_announce_t*)&pkt->payload[sizeof(links_update_t)];
   header->code = 0x21;
   header->flags = 0x0;
   header->neighbors_count = num;
   header->m_left = from[0];
   header->m_right = from[1];

   dprintf(fd, "\t - ss->code            = %02x\n"
               "\t - ss->flags           = %02x\n"
               "\t - ss->neighbors_count = %02x\n"
               "\t - ss->m_left          = %02x\n"
               "\t - ss->m_right         = %02x\n",
               header->code,header->flags,header->neighbors_count,
               header->m_left,header->m_right
  );
  if(parent_exists == TRUE){
     header->parent_exist = 1;
     tmp = neighbors_get(p_index);
     header->p_left = tmp->addr_64b[6]; // left
     header->p_right = tmp->addr_64b[7]; // right
  }

  dprintf(fd, "\t - ss->parent_exist     = %02x\n"
              "\t - ss->p_left           = %02x\n"
              "\t - ss->p_right          = %02x\n\n",
              header->parent_exist,header->p_left,header->p_right
 );
   int j = 0;
   for (i = 0; i < 30 && num > 0; i++) {
       if(neighbors_getUsed(i)){
           num--;
           tmp = neighbors_get(i);
           content[j].l_to = tmp->addr_64b[6];
           content[j].r_to = tmp->addr_64b[7];
           content[j].rssi = neighbors_getRssi(i);
           j++;
           dprintf(fd, "\t\t - ss->l_to     = %02x\n"
                       "\t\t - ss->r_to     = %02x\n"
                       "\t\t - ss->rssi     = %02x\n\n",
                       content[j].l_to,content[j].r_to,content[j].rssi
          );
       }

   }


   dprintf(fd,"[+] SENT :\n\t\t ");
   for (size_t i = 0; i < pkt->length; i++) {
      dprintf(fd,"%02x ",pkt->payload[i]);
   }


   dprintf(fd,"[+] COPIED BUFFER2\n");
   // location-path option
   options[0].type = COAP_OPTION_NUM_URIPATH;
   options[0].length = sizeof(cexample_path0) - 1;
   options[0].pValue = (uint8_t *) cexample_path0;
   //metada
   pkt->l4_destination_port   = WKP_UDP_COAP;
   pkt->l3_destinationAdd.type = ADDR_128B;
    // set destination address here

   memcpy(&pkt->l3_destinationAdd.addr_128b[0],&m[0],16);
   dprintf(fd,"[+] INIT DESTINATION2\n");

   //send
   outcome = opencoap_send(
           pkt,
           COAP_TYPE_NON,
           COAP_CODE_REQ_PUT,
           1, // token len
           options,
           1, // options len
           &cexample_vars.desc
           );
   if (outcome == E_FAIL) {
     openqueue_freePacketBuffer(pkt);
     dprintf(fd,"[+] PACKET FAILED2\n");
     return ;
   }
   dprintf(fd,"[+] PACKET WAS SENT2\n");

}
void send6t(void){

}
