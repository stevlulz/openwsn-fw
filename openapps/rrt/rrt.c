/**
\brief A CoAP resource which indicates the board its running on.
*/


#include "opendefs.h"
#include "rrt.h"
#include "sixtop.h"
#include "idmanager.h"
#include "openqueue.h"
#include "neighbors.h"
#include "packetfunctions.h"
#include "leds.h"
#include "openserial.h"



#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
//=========================== defines =========================================

const uint8_t rrt_path0[] = "rt";
const uint8_t tasa[] = "tasa";

//=========================== variables =======================================

rrt_vars_t rrt_vars;
static uint8_t sid[16] = {0xbb, 0xbb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};

//=========================== prototypes ======================================

owerror_t     rrt_receive(
        OpenQueueEntry_t* msg,
        coap_header_iht*  coap_header,
        coap_option_iht*  coap_incomingOptions,
        coap_option_iht*  coap_outgoingOptions,
        uint8_t*          coap_outgoingOptionsLen
);
void          rrt_sendDone(
   OpenQueueEntry_t* msg,
   owerror_t error
);
void rrt_setGETRespMsg(
   OpenQueueEntry_t* msg,
   uint8_t discovered   
);

void rrt_sendCoAPMsg(char actionMsg, uint8_t *ipv6mote);
void   rrt_timer_cb(opentimers_id_t id);
//=========================== public ==========================================

/**
\brief Initialize this module.
*/
void rrt_init(void) {
   
   // do not run if DAGroot
   if(idmanager_getIsDAGroot()==TRUE) return; 


   char file[50];
   memset(file,0,50);
   open_addr_t* addr = idmanager_getMyID(ADDR_16B);

   unsigned int l = ((int)addr->addr_16b[0]);
   unsigned int r = ((int)addr->addr_16b[1]);
   if(r == 0x1 && l == 0x0){
      return;
   }
   snprintf(file,50,"/home/stevlulz/Desktop/rtt.%d.%d.txt",l,r);
   rrt_vars.fd = open(file,O_CREAT | O_RDWR);
   int fd = rrt_vars.fd;
   dprintf(fd,"INIT\n");


   // prepare the resource descriptor for the /rt path
   rrt_vars.desc.path0len             = sizeof(rrt_path0)-1;
   rrt_vars.desc.path0val             = (uint8_t*)(&rrt_path0);
   rrt_vars.desc.path1len             = 0;
   rrt_vars.desc.path1val             = NULL;
   rrt_vars.desc.componentID          = COMPONENT_RRT;
   rrt_vars.desc.securityContext      = NULL;
   rrt_vars.desc.discoverable         = TRUE;
   rrt_vars.desc.callbackRx           = &rrt_receive;
   rrt_vars.desc.callbackSendDone     = &rrt_sendDone;
   rrt_vars.start = 0;


   rrt_vars.discovered                = 0; //if this mote has been discovered by ringmaster
   rrt_vars.timerId    = opentimers_create(TIMER_GENERAL_PURPOSE, TASKPRIO_COAP);
   opentimers_scheduleIn(
        rrt_vars.timerId,
        1000,
        TIME_MS,
        TIMER_PERIODIC,
        rrt_timer_cb
   );
   // register with the CoAP module
   opencoap_register(&rrt_vars.desc);
}

//=========================== private =========================================



void   rrt_timer_cb(opentimers_id_t id){
    int fd = rrt_vars.fd;
    if(rrt_vars.start == 0){
        dprintf(fd,"NOT STARTED YET\n");
        return;
    }

    dprintf(fd,"TIMER\n");

    rrt_sendCoAPMsg(0,0);

}

/**
\brief Called when a CoAP message is received for this resource.

\param[in] msg          The received message. CoAP header and options already
   parsed.
\param[in] coap_header  The CoAP header contained in the message.
\param[in] coap_options The CoAP options contained in the message.

\return Whether the response is prepared successfully.
*/
owerror_t     rrt_receive(
        OpenQueueEntry_t* msg,
        coap_header_iht*  coap_header,
        coap_option_iht*  coap_incomingOptions,
        coap_option_iht*  coap_outgoingOptions,
        uint8_t*          coap_outgoingOptionsLen
) {
   owerror_t outcome;
   uint8_t mssgRecvd;
   uint8_t moteToSendTo[16];
   uint8_t actionToFwd;

   switch (coap_header->Code) {
      case COAP_CODE_REQ_GET:
      case COAP_CODE_REQ_PUT:
      case COAP_CODE_REQ_POST:
      case COAP_CODE_REQ_DELETE:
         msg->payload                     = &(msg->packet[127]);
         msg->length                      = 0;
        int fd = rrt_vars.fd;
        dprintf(fd,"STARTED\n");
         // set the CoAP header
         coap_header->Code                = COAP_CODE_RESP_CONTENT;
         rrt_vars.start = 1;
         outcome                          = E_SUCCESS;
         break;

      default:
         // return an error message
         outcome = E_FAIL;
   }
   
   return outcome;
}

void rrt_setGETRespMsg(OpenQueueEntry_t* msg, uint8_t count) {
    struct timeval  tv;
    gettimeofday(&tv, NULL);
    msg->payload[0] = 120;
    long time_in_mill = (tv.tv_sec) * 1000 + (tv.tv_usec) ;
    memset((char*)&msg->payload[1],0,count-1);
    int queue_size = openqueue_getNumEmpty();
    int fd = rrt_vars.fd;
    dprintf(fd,"TIME %ld , QUEUE : %d\n",time_in_mill,queue_size);
    snprintf((char*)&msg->payload[1],(size_t)count,"%ld;%u",time_in_mill,queue_size);

}

/**
 * if mote is 0, then send to the ringmater, defined by ipAddr_ringmaster
**/
void rrt_sendCoAPMsg(char actionMsg, uint8_t *ipv6mote) {
      OpenQueueEntry_t* pkt;
      owerror_t outcome;
      coap_option_iht options[2];
      uint8_t medType;
          int fd = rrt_vars.fd;


      pkt = openqueue_getFreePacketBuffer(COMPONENT_RRT);
      if (pkt == NULL) {
          openserial_printError(COMPONENT_RRT,ERR_BUSY_SENDING,
                                (errorparameter_t)0,
                                (errorparameter_t)0);
          return;
      }
      dprintf(fd,"FOUND FREE BUFFEER\n");
      pkt->creator   = COMPONENT_RRT;
      pkt->owner      = COMPONENT_RRT;
      pkt->l4_protocol  = IANA_UDP;

      packetfunctions_reserveHeaderSize(pkt, 25);
      rrt_setGETRespMsg(pkt,25);
      // location-path option
      options[0].type = COAP_OPTION_NUM_URIPATH;
      options[0].length = sizeof(tasa) - 1;
      options[0].pValue = (uint8_t *) tasa;
      
       // content-type option
      medType = COAP_MEDTYPE_APPOCTETSTREAM;
      options[1].type = COAP_OPTION_NUM_CONTENTFORMAT;
      options[1].length = 1;
      options[1].pValue = &medType;

      //metada
      pkt->l4_destination_port   = WKP_UDP_COAP;
      pkt->l3_destinationAdd.type = ADDR_128B;
      // set destination address here
      memcpy(&pkt->l3_destinationAdd.addr_128b[0], &sid, 16);

      dprintf(fd,"COPIED ADDR\n");
      //send
      outcome = opencoap_send(
              pkt,
              COAP_TYPE_NON,
              COAP_CODE_REQ_PUT,
              1, // token len
              options,
              2, // options len
              &rrt_vars.desc
              );
      dprintf(fd,"SENT\n");
      if (outcome == E_FAIL) {
        openqueue_freePacketBuffer(pkt);
      }
}

/**
\brief The stack indicates that the packet was sent.

\param[in] msg The CoAP message just sent.
\param[in] error The outcome of sending it.
*/
void rrt_sendDone(OpenQueueEntry_t* msg, owerror_t error) {
   openqueue_freePacketBuffer(msg);
}
