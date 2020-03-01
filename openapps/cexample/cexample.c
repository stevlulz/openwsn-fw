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
#define CEXAMPLE_NOTIFY_PERIOD  90000
#define CEXAMPLE_UPDATE_PERIOD  90000

const uint8_t cexample_path0[] = "tasa";

const uint8_t c6Y[] = "6t";

static uint8_t m[16] = {0xbb, 0xbb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x14, 0x15, 0x92, 0xcc, 0x00, 0x00, 0x00, 0x02};
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
    char file[50];
    memset(file,0,50);
    open_addr_t* addr = idmanager_getMyID(ADDR_16B);

    unsigned int l = ((int)addr->addr_16b[0]);
    unsigned int r = ((int)addr->addr_16b[1]);
    snprintf(file,50,"/home/stevlulz/Desktop/cexample_log.%d.%d.txt",l,r);
    cexample_vars.fd = open(file,O_CREAT | O_RDWR);
    int fd = cexample_vars.fd;
    if(r != 0x2 || l != 0x0){
        if(r == 0x1 && l == 0x0){
          dprintf(fd,"[-]{%d.%d} GATEWAY\n",l,r);
          return ;
        }
        dprintf(fd,"[-]{%d.%d} NON-CENTRAL node\n",l,r);
        cexample_vars.timerId    = opentimers_create(TIMER_GENERAL_PURPOSE, TASKPRIO_COAP);
        cexample_vars.join = 0;
        opentimers_scheduleIn(
            cexample_vars.timerId,
            CEXAMPLE_NOTIFY_PERIOD,
            TIME_MS,
            TIMER_PERIODIC,
            cexample_timer_cb
      );
        return ;
    }
    else
      dprintf(fd,"[+]{%d.%d} CENTRAL \n",l,r);


    cexample_vars.desc.path0len             = sizeof(cexample_path0)-1;
    cexample_vars.desc.path0val             = (uint8_t*)(&cexample_path0);
    cexample_vars.desc.path1len             = 0;
    cexample_vars.desc.path1val             = NULL;
    cexample_vars.desc.componentID          = COMPONENT_CEXAMPLE;
    cexample_vars.desc.securityContext      = NULL;
    cexample_vars.desc.discoverable         = TRUE;
    cexample_vars.desc.callbackRx           = &cexample_receive;
    cexample_vars.desc.callbackSendDone     = &cexample_sendDone;

    cexample_vars.old = 1;
    opencoap_register(&cexample_vars.desc);
    cexample_init_graph();
    cexample_log_topo();
    cexample_vars.timerId    = opentimers_create(TIMER_GENERAL_PURPOSE, TASKPRIO_COAP);
    opentimers_scheduleIn(
        cexample_vars.timerId,
        CEXAMPLE_UPDATE_PERIOD,
        TIME_MS,
        TIMER_PERIODIC,
        cexample_timer_cb2
  );

}

//=========================== private =========================================

void cexample_init_graph(void){
    for (size_t i = 0; i < GRAPH_SIZE+1; i++)
      for (size_t j = 0; j < GRAPH_SIZE+1; j++){
        cexample_vars.v[i][j].link = 0;
        cexample_vars.v[i][j].is_parent = 0;
        cexample_vars.v[i][j].rssi = 0;

      }
    for (size_t i = 0; i < GRAPH_SIZE+1; i++){
      cexample_vars.v[0][i].link = 0;
      cexample_vars.v[0][i].is_parent = 0;
      cexample_vars.v[0][i].rssi = 0;
    }
    for (size_t i = 0; i < GRAPH_SIZE+1; i++){
      cexample_vars.v[i][0].link = 0;
      cexample_vars.v[i][0].is_parent = 0;
      cexample_vars.v[i][0].rssi = 0;
    }

}
void   cexample_log_topo(void){
  int fd = cexample_vars.fd;
  dprintf(fd,"----------------------TOPOLOGY-START---------------------\n");
  for (size_t i = 1; i < GRAPH_SIZE+1; i++) {
    for (size_t j = 1; j < GRAPH_SIZE+1; j++){
      dprintf(fd, "(%01x,%01x,%2d) ",cexample_vars.v[i][j].link,cexample_vars.v[i][j].is_parent,cexample_vars.v[i][j].rssi);
    }
    dprintf(fd,"\n");
  }
  dprintf(fd,"----------------------TOPOLOGY-END---------------------\n");
}
void   cexample_parse(OpenQueueEntry_t* msg){
  int fd = cexample_vars.fd;
  uint8_t* input = msg->payload;
  open_addr_t srcAdd = msg->l3_sourceAdd;
  uint8_t *a = (uint8_t*)&srcAdd.addr_128b[0];
  uint8_t code;
  uint16_t to_node,from_node;
  link_anounce link;
  uint8_t count;
  bool parent_exist;
  links_update_t * rec_links;
  link_announce_t * up_links;
  dprintf(fd,"[+] FROM {%02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x}\n",a[120],a[121],a[122],a[123],a[124],a[125],a[126],a[127]);

  uint8_t len = msg->length;

  dprintf(fd,"\t-------------------------------\n");
  for (uint8_t j = 0; j < len; j++) {
    if(j % 12 == 0)
      dprintf(fd,"\n\t");
    dprintf(fd,"%02x ",msg->payload[j]);

  }
  dprintf(fd,"\n\t-------------------------------\n");
  do {
    code = input[0];
    dprintf(fd,"\t[+] CODE : 0x%02x\n",code);
    switch (code) {
      case ADD_NEIGHBOR:
        if(input[6] != 0xff){
            dprintf(fd,"\t[-] Parsing error {expected : 0xff -- found : 0x%02x}\n",input[6]);
            return;
        }
        link = *((link_anounce*)&input[1]);
        from_node = link.l_from*0xff + link.r_from;
        to_node = link.l_to*0xff + link.r_to;
        uint8_t i;
        for ( i = 1; i < GRAPH_SIZE; i++) {
             cexample_vars.v[from_node][i].link = 0;
             cexample_vars.v[from_node][i].rssi = 0;
        }
        cexample_vars.v[from_node][to_node].link = 1;
        cexample_vars.v[from_node][to_node].rssi = link.rssi;
        dprintf(fd,"\t[+] Link added from %02x %02x to %02x %02x  -- from %d -- %d -- Rssi : %d\n",input[1],input[2],input[3],input[4],from_node,to_node,input[5]);
        input = &input[6];
        cexample_vars.old = 0;
        break;
     case ADD_NEIGHBORS:
      rec_links = (links_update_t *)&input[0];
      dprintf(fd,"\t[+] Adding neighbors\n");
      count = rec_links->neighbors_count;
      parent_exist = rec_links->parent_exist;
      uint16_t from = rec_links->m_left*0xff + rec_links->m_right;
      dprintf(fd,"\t\t[+] FROM         : %02x\n",from);
      dprintf(fd,"\t\t[+] COUNT        : %02x\n",count);
      dprintf(fd,"\t\t[+] PARENT_EXIST : %d\n",parent_exist);

      uint16_t parent = rec_links->p_left*0xff + rec_links->p_right;
      dprintf(fd,"\t\t[+] PARENT       : %02x\n",parent);

      for ( i = 1; i < GRAPH_SIZE; i++) {
           cexample_vars.v[from][i].link = 0;
           cexample_vars.v[from][i].rssi = 0;
           //if(parent_exist)
           cexample_vars.v[from][i].is_parent = 0;
      }
      up_links =(link_announce_t*) &input[7];
      for (uint8_t i = 0; i < count; i++) {
          uint16_t to_tmp = up_links[i].l_to*0xff + up_links[i].r_to;
          int8_t rsi_tmp =  up_links[i].rssi;
          cexample_vars.v[from][to_tmp].rssi = rsi_tmp;
          cexample_vars.v[from][to_tmp].link = 1;
          if(parent_exist > 0 && parent == to_tmp )
              cexample_vars.v[from][to_tmp].is_parent = 1;
          dprintf(fd,"\t\t[+] TO : %02x   -- rssi : %d  \n",to_tmp,rsi_tmp);
      }
      input = &input[7+count*sizeof(link_announce_t)];
      dprintf(fd,"\t[+] Added links ----\n");
      cexample_vars.old = 0;
      break;
     default:
        dprintf(fd,"\t[+] MessageType does not exists %02x\n",code);
        return;
    }

  } while(*input != 0xff);
  cexample_log_topo();
}
owerror_t cexample_receive( OpenQueueEntry_t* msg,
        coap_header_iht*  coap_header,
        coap_option_iht*  coap_incomingOptions,
        coap_option_iht*  coap_outgoingOptions,
        uint8_t*          coap_outgoingOptionsLen){

        owerror_t outcome;
        int fd = cexample_vars.fd;
        dprintf(fd,"[+] RECEIVED A MESSAGE code:{");

        switch (coap_header->Code) {
           case COAP_CODE_REQ_PUT:
              dprintf(fd,"COAP_CODE_REQ_PUT}\n");
              //dprintf(fd,"\t--> payload :[ %s ]\n",msg->payload);
              cexample_parse(msg);
              msg->payload                     = &(msg->packet[127]);
              msg->length                      = 0;
              // set the CoAP header
              coap_header->Code                = COAP_CODE_RESP_CREATED;
              outcome                          = E_SUCCESS;
              break;
           default:
              dprintf(fd,"DEFAULT TO DO}\n");
              outcome                          = E_FAIL;
              break;
        }

          return outcome;

}
void cexample_sendDone(OpenQueueEntry_t* msg, owerror_t error) {
    openqueue_freePacketBuffer(msg);
}

void cexample_timer_cb(opentimers_id_t id){
    //if (idmanager_getIsDAGroot() == TRUE)
    //  return ;
    // nodes
    int fd = cexample_vars.fd;

    if (ieee154e_isSynch()==FALSE) {
        dprintf(fd,"[-]not synch\n");
        return;
    }
    dprintf(fd,"[-]synch\n");

    if(cexample_vars.join == 0){
      request_first_join();
    }
    cexample_task_cb();
}
void cexample_timer_cb2(opentimers_id_t id){
    //if (idmanager_getIsDAGroot() == TRUE)
    //  return ;
    int fd = cexample_vars.fd;


    //GATEWAY
    /*
    if (ieee154e_isSynch()==FALSE) {
        dprintf(fd,"[+] NOT SYNCH\n");
        return;
    }
    */

    if(cexample_vars.join == 0){
      request_first_join();
    }
    for (size_t i = 1; i < GRAPH_SIZE; i++) {
      if(cexample_vars.v[i][5].link == 1 || cexample_vars.v[5][i].link == 1 )
        goto here;
    }

    dprintf(fd,"[+] havent got 5 link\n");

    return ;
    here :
    dprintf(fd,"[+] send 6t\n");
    send6t();
}

void request_first_join(void){
  int fd = cexample_vars.fd;
  open_addr_t    parentNeighbor;
  open_addr_t    nonParentNeighbor;
  cellInfo_ht    celllist_add[CELLLIST_MAX_LEN];
  bool           foundNeighbor;

  foundNeighbor = icmpv6rpl_getPreferredParentEui64(&parentNeighbor);
  if (foundNeighbor==FALSE) {
      dprintf(fd,"[-] No parent found\n");
      return;
  }
  dprintf(fd,"[+] have parent\n");

  if (schedule_hasNegotiatedTxCellToNonParent(&parentNeighbor, &nonParentNeighbor)==TRUE){

      // send a clear request to the non-parent neighbor
      dprintf(fd,"[-] hasNegotiatedTxCellToNonParent \n");

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


  if (schedule_getNumberOfNegotiatedCells(&parentNeighbor, CELLTYPE_TX)==0){

      if (msf_candidateAddCellList(celllist_add,NUMCELLS_MSF)==FALSE){
          // failed to get cell list to add
          dprintf(fd,"[+] failed to get cell list to add \n");

          return;
      }
      dprintf(fd,"[+] negotiated with parent \n");

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
   cexample_vars.join = 1;

}

int count = 0;
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
              COAP_TYPE_CON,
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
           COAP_TYPE_CON,
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


void cexample_task_cb(void){
    int fd = cexample_vars.fd;
    if (ieee154e_isSynch()==FALSE) {
      dprintf(fd,"[+] {%d} NOT SYNCH\n",count++);
      return;
    }
    dprintf(fd,"[+] {%d} cexample_task_cb SEND UPDATE\n",count++);
    cexample_send_link_update();
    return;
}


void send6t(void){
  int fd = cexample_vars.fd;

  dprintf(fd,"[+] SEND6T\n");
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
  dprintf(fd,"[+] 6T GOT FREE PACKET\n");

  pkt->creator   = COMPONENT_CEXAMPLE;
  pkt->owner      = COMPONENT_CEXAMPLE;
  pkt->l4_protocol  = IANA_UDP;
  packetfunctions_reserveHeaderSize(pkt,9);
  //0x02,0x0A,0x00,0xA,0x00,0xB,0x00,0xB,0x00

  pkt->payload[0] = 0x2;
  pkt->payload[1] = 0xF;
  pkt->payload[2] = 0x0;
  pkt->payload[3] = 0xF;
  pkt->payload[4] = 0x0;
  pkt->payload[5] = 0xC;
  pkt->payload[6] = 0x0;
  pkt->payload[7] = 0xC;
  pkt->payload[8] = 0x0 ;


  // location-path option
  options[0].type = COAP_OPTION_NUM_URIPATH;
  options[0].length = sizeof(c6Y) - 1;
  options[0].pValue = (uint8_t *) c6Y;
  //metada
  pkt->l4_destination_port   = WKP_UDP_COAP;
  pkt->l3_destinationAdd.type = ADDR_128B;
   // set destination address here

  memcpy(&pkt->l3_destinationAdd.addr_128b[0],&m[0],16);
  pkt->l3_destinationAdd.addr_128b[15] = 5;
  dprintf(fd,"[+] 6T INIT DESTINATION\n");

  //send
  outcome = opencoap_send(
          pkt,
          COAP_TYPE_CON,
          COAP_CODE_REQ_PUT,
          1, // token len
          options,
          1, // options len
          &cexample_vars.desc
          );
  if (outcome == E_FAIL) {
    openqueue_freePacketBuffer(pkt);
    dprintf(fd,"[+] 6T PACKET FAILED\n");
    return ;
  }
  dprintf(fd,"[+] 6T PACKET WAS SENT\n");


}
