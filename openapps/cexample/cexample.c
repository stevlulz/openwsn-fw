/**
\brief An example CoAP application.
*/

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
#define CEXAMPLE_NOTIFY_PERIOD  60000
#define CEXAMPLE_UPDATE_PERIOD  10000

const uint8_t cexample_path0[] = "tasa";

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

void   cexample_init_graph(void);
void   cexample_log_topo(void);
void   cexample_parse(OpenQueueEntry_t* msg);


void   cexample_timer_cb(opentimers_id_t id);
void   cexample_task_cb(void);
void   send_msg(void);
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
        if(r == 0x0 && l == 0x0){
          dprintf(fd,"[-]{%d.%d} ROOT(gateway) node\n",l,r);
          return ;
        }
        dprintf(fd,"[-]{%d.%d} NON-CENTRAL node\n",l,r);
        cexample_vars.timerId    = opentimers_create(TIMER_GENERAL_PURPOSE, TASKPRIO_COAP);
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
      dprintf(fd,"[+]{%d.%d} CENTRAL NODE\n",l,r);


    cexample_vars.desc.path0len             = sizeof(cexample_path0)-1;
    cexample_vars.desc.path0val             = (uint8_t*)(&cexample_path0);
    cexample_vars.desc.path1len             = 0;
    cexample_vars.desc.path1val             = NULL;
    cexample_vars.desc.componentID          = COMPONENT_CEXAMPLE;
    cexample_vars.desc.securityContext      = NULL;
    cexample_vars.desc.discoverable         = TRUE;
    cexample_vars.desc.callbackRx           = &cexample_receive;
    cexample_vars.desc.callbackSendDone     = &cexample_sendDone;


    opencoap_register(&cexample_vars.desc);
    cexample_init_graph();
    cexample_log_topo();
    /*
    opentimers_scheduleIn(
        icmpv6rpl_vars.timerIdDIO,
        SLOTFRAME_LENGTH*SLOTDURATION,
        TIME_MS,
        TIMER_PERIODIC,
        icmpv6rpl_timer_DIO_cb
    );
    */
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
  for (size_t i = 0; i < GRAPH_SIZE+1; i++) {
    for (size_t j = 0; j < GRAPH_SIZE+1; j++){
      dprintf(fd, "(%01d,%02d) ",cexample_vars.v[i][j].link,cexample_vars.v[i][j].rssi);
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
  dprintf(fd,"[+] FROM {%02x.%02x.%02x.%02x.%02x.%02x.%02x.%02x}\n",a[120],a[121],a[122],a[123],a[124],a[125],a[126],a[127]);

  do {
    code = input[0];
    switch (code) {
      case ADD_NEIGHBOR:
        if(input[6] != 0xff){
            dprintf(fd,"\t[-] Parsing error {expected : 0xff -- found : 0x%02x}\n",input[6]);
            return;
        }
        from_node = input[1]*0xff + input[2];
        to_node = input[3]*0xff + input[4];
        uint8_t i;
        for ( i = 1; i < GRAPH_SIZE; i++) {
             cexample_vars.v[from_node][i].link = 0;
             cexample_vars.v[from_node][i].rssi = 0;
        }
        cexample_vars.v[from_node][to_node].link = 1;
        cexample_vars.v[from_node][to_node].rssi = (int8_t)input[5];
        dprintf(fd,"\t[+] Link added from %02x %02x to %02x %02x  -- from %d -- %d -- Rssi : %d\n",input[1],input[2],input[3],input[4],from_node,to_node,input[5]);
        input = &input[6];
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
    if (idmanager_getIsDAGroot() == TRUE)
      return ;

    cexample_task_cb();
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
    open_addr_t* tmp = NULL;
    if(num == 0){
      dprintf(fd,"\t[+] I DO NOT HAVE NEIGHBORS \n");
      return;
    }
    if(num == 1){
      uint8_t i;
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
