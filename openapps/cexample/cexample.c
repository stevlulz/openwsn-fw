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


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
//=========================== defines =========================================


const uint8_t cexample_path0[] = "tasa";

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
//=========================== public ==========================================

void cexample_init(void) {
    // check if am centrilized node ??
    char file[50];
    memset(file,0,50);
    open_addr_t* addr = idmanager_getMyID(ADDR_16B);

    unsigned int l = ((int)addr->addr_16b[0]);
    unsigned int r = ((int)addr->addr_16b[1]);
    snprintf(file,50,"/home/stevlulz/Desktop/log.%d.%d.txt",l,r);
    cexample_vars.fd = open(file,O_CREAT | O_RDWR);
    int fd = cexample_vars.fd;
    if(r != 0x2 || l != 0x0){
        dprintf(fd,"[-]{%d.%d} NON-CENTRAL node\n",l,r);
        return ;
    }
    dprintf(fd,"[+]{%d.%d} CENTRAL NODE\n",l,r);


    // prepare the resource descriptor for the /ex path
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
              // change the LED's state
              dprintf(fd,"\t--> payload :[ %s ]\n",msg->payload);
              /*
              if (msg->payload[0]=='1') {
                 leds_error_on();
              } else if (msg->payload[0]=='2') {
                 leds_error_toggle();
              } else {
                 leds_error_off();
              }
              */
              // reset packet payload
              msg->payload                     = &(msg->packet[127]);
              msg->length                      = 0;

              // set the CoAP header
              coap_header->Code                = COAP_CODE_RESP_CHANGED;
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
