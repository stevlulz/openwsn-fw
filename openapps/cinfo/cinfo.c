/**
\brief A CoAP resource which indicates the board its running on.
*/

#include "opendefs.h"
#include "cinfo.h"
#include "opencoap.h"
#include "openqueue.h"
#include "packetfunctions.h"
#include "openserial.h"
#include "openrandom.h"
#include "board.h"
#include "idmanager.h"
#define CINFO_NOTIFY_PERIOD 333

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>


//=========================== defines =========================================

const uint8_t cinfo_path0[] = "i";

//=========================== variables =======================================

cinfo_vars_t cinfo_vars;

//=========================== prototypes ======================================

owerror_t     cinfo_receive(
        OpenQueueEntry_t* msg,
        coap_header_iht*  coap_header,
        coap_option_iht*  coap_incomingOptions,
        coap_option_iht*  coap_outgoingOptions,
        uint8_t*          coap_outgoingOptionsLen
);
void          cinfo_sendDone(
   OpenQueueEntry_t* msg,
   owerror_t error
);

void   cinfo_timer_cb(opentimers_id_t id);
static uint8_t sid[16] = {0xbb, 0xbb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02};
//=========================== public ==========================================

/**
\brief Initialize this module.
*/
void cinfo_init(void) {
   // do not run if DAGroot
   if(idmanager_getIsDAGroot()==TRUE) return; 

   char file[50];
   memset(file,0,50);
   open_addr_t* addr = idmanager_getMyID(ADDR_16B);

   unsigned int l = ((int)addr->addr_16b[0]);
   unsigned int r = ((int)addr->addr_16b[1]);
   snprintf(file,50,"/home/stevlulz/Desktop/info.%d.%d.txt",l,r);
   cinfo_vars.fd = open(file,O_CREAT | O_RDWR);
   int fd = cinfo_vars.fd;

   dprintf(fd,"INIT\n");
   cinfo_vars.count = 0;
   // prepare the resource descriptor for the /i path
   cinfo_vars.desc.path0len             = sizeof(cinfo_path0)-1;
   cinfo_vars.desc.path0val             = (uint8_t*)(&cinfo_path0);
   cinfo_vars.desc.path1len             = 0;
   cinfo_vars.desc.path1val             = NULL;
   cinfo_vars.desc.componentID          = COMPONENT_CINFO;
   cinfo_vars.desc.securityContext      = NULL;
   cinfo_vars.desc.discoverable         = TRUE;
   cinfo_vars.desc.callbackRx           = &cinfo_receive;
   cinfo_vars.desc.callbackSendDone     = &cinfo_sendDone;

   cinfo_vars.start = 0;
   cinfo_vars.timerId    = opentimers_create(TIMER_GENERAL_PURPOSE, TASKPRIO_COAP);
   opentimers_scheduleIn(
        cinfo_vars.timerId,
        CINFO_NOTIFY_PERIOD,
        TIME_MS,
        TIMER_PERIODIC,
        cinfo_timer_cb
   );
   // register with the CoAP module
   opencoap_register(&cinfo_vars.desc);
}

void cinfo_timer_cb(opentimers_id_t id){

    int fd = cinfo_vars.fd;
    dprintf(fd,"[+]---------->TIMER\n");
   // int fd = cinfo_vars.fd;
    if (ieee154e_isSynch()==FALSE) {
        dprintf(fd,"[-]not synch\n");
        return;
    }
    if(cinfo_vars.start == 0){
        dprintf(fd,"[!]not started yet\n");
        return;
    }

    cinfo_vars.count++;

    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    char t[100];
    memset(t,0,100);

    long now = ts.tv_sec * 1000000000L + ts.tv_nsec;
    snprintf(t,100,"TIME : %ld\nTMP : %d\nQUEUE_SIZE : %d \nSENT : %d\nDROPPED : %d",now,cinfo_vars.count,cinfo_vars.count,cinfo_vars.count,cinfo_vars.count);


     OpenQueueEntry_t* pkt;
     owerror_t outcome;
     coap_option_iht options[1];


     pkt = openqueue_getFreePacketBuffer(COMPONENT_CINFO);
     if (pkt == NULL) {
          openserial_printError(COMPONENT_CINFO,ERR_BUSY_SENDING,
                                (errorparameter_t)0,
                                (errorparameter_t)0);
          return;
     }
      dprintf(fd,"[+] GOT FREE PACKET\n");

      pkt->creator   = COMPONENT_CINFO;
      pkt->owner      = COMPONENT_CINFO;
      pkt->l4_protocol  = IANA_UDP;

      packetfunctions_reserveHeaderSize(pkt,100);

      //pkt->payload[0] = 0x20; // add link code
      memcpy(&pkt->payload[0],&t[0],100);

      dprintf(fd,"[+] COPIED BUFFER\n");
      // location-path option
      options[0].type = COAP_OPTION_NUM_URIPATH;
      options[0].length = sizeof(cinfo_path0) - 1;
      options[0].pValue = (uint8_t *) cinfo_path0;
      //metada
      pkt->l4_destination_port   = WKP_UDP_COAP;
      pkt->l3_destinationAdd.type = ADDR_128B;
       // set destination address here

      memcpy(&pkt->l3_destinationAdd.addr_128b[0],&sid[0],16);
      dprintf(fd,"[+] INIT DESTINATION\n");

      //send
      outcome = opencoap_send(
              pkt,
              COAP_TYPE_NON,
              COAP_CODE_REQ_PUT,
              1, // token len
              options,
              1, // options len
              &cinfo_vars.desc
              );
       dprintf(fd,"[+] SEND DONE\n");
      if (outcome == E_FAIL) {
        openqueue_freePacketBuffer(pkt);
        dprintf(fd,"[+] PACKET FAILED\n");
        return ;
      }
      dprintf(fd,"[+] PACKET WAS SENT\n");

}
//=========================== private =========================================

/**
\brief Called when a CoAP message is received for this resource.

\param[in] msg          The received message. CoAP header and options already
   parsed.
\param[in] coap_header  The CoAP header contained in the message.
\param[in] coap_options The CoAP options contained in the message.

\return Whether the response is prepared successfully.
*/
owerror_t cinfo_receive(
        OpenQueueEntry_t* msg,
        coap_header_iht*  coap_header,
        coap_option_iht*  coap_incomingOptions,
        coap_option_iht*  coap_outgoingOptions,
        uint8_t*          coap_outgoingOptionsLen
) {
   owerror_t outcome;

    int fd = cinfo_vars.fd;
    dprintf(fd,"[+]---------->RECEIVE\n");
   switch (coap_header->Code) {
      case COAP_CODE_REQ_POST:
         //=== reset packet payload (we will reuse this packetBuffer)
         msg->payload                     = &(msg->packet[127]);
         msg->length                      = 0;
         cinfo_vars.start = 1;
         coap_header->Code                = COAP_CODE_RESP_CONTENT;
         int fd = cinfo_vars.fd;
         dprintf(fd,"[+]---------->TURN ON\n");
         
         outcome                          = E_SUCCESS;
         break;
      default:
         // return an error message
         outcome = E_FAIL;
   }
   
   return outcome;
}

/**
\brief The stack indicates that the packet was sent.

\param[in] msg The CoAP message just sent.
\param[in] error The outcome of sending it.
*/
void cinfo_sendDone(OpenQueueEntry_t* msg, owerror_t error) {

    int fd = cinfo_vars.fd;
    dprintf(fd,"[+]---------->SEND DONE\n");
   openqueue_freePacketBuffer(msg);
}
