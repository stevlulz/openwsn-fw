#include "opendefs.h"
#include "uecho.h"
#include "openqueue.h"
#include "openserial.h"
#include "packetfunctions.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
//=========================== variables =======================================

uecho_vars_t uecho_vars;
static uint8_t sid[16] = {0xbb, 0xbb, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
//=========================== prototypes ======================================
void   echo_timer_cb(opentimers_id_t id);
//=========================== public ==========================================

void uecho_init(void) {
   // clear local variables


    open_addr_t     temp_neighbor;

    memset(&temp_neighbor,0,sizeof(temp_neighbor));

    open_addr_t* addr = idmanager_getMyID(ADDR_16B);

    unsigned int l = ((int)addr->addr_16b[0]);
    unsigned int r = ((int)addr->addr_16b[1]);
    if(r == 0x1 && l == 0x0){
      return;
    }
    char file[50];
    memset(file,0,50);

    snprintf(file,50,"/home/stevlulz/Desktop/echo.%d.%d.txt",l,r);
    uecho_vars.fd = open(file,O_CREAT | O_RDWR);
    int fd = uecho_vars.fd;
    dprintf(fd,"[+] INIT\n");


   memset(&uecho_vars,0,sizeof(uecho_vars_t));
    uecho_vars.go = 0;
    uecho_vars.start = 0;
    uecho_vars.counter = 1;

   // register at UDP stack
   uecho_vars.desc.port              = WKP_UDP_ECHO;
   uecho_vars.desc.callbackReceive   = &uecho_receive;
   uecho_vars.desc.callbackSendDone  = &uecho_sendDone;
   uecho_vars.timerId = opentimers_create(TIMER_GENERAL_PURPOSE, TASKPRIO_COAP);
   opentimers_scheduleIn(
        uecho_vars.timerId,
        1300,
        TIME_MS,
        TIMER_PERIODIC,
        echo_timer_cb
   );
   openudp_register(&uecho_vars.desc);
}

void uecho_receive(OpenQueueEntry_t* request) {

    int fd = uecho_vars.fd;
    dprintf(fd,"[+] uecho_receive\n");

    uecho_vars.start = 1;

   openqueue_freePacketBuffer(request);
   

}
void   echo_timer_cb(opentimers_id_t id){

    if(uecho_vars.start == 0)
        return ;

    uecho_vars.go++;
    if(uecho_vars.go < 100)
        return;
   uint16_t          temp_l4_destination_port;
   OpenQueueEntry_t* reply;

   reply = openqueue_getFreePacketBuffer(COMPONENT_UECHO);
   if (reply==NULL) {
      openserial_printError(
         COMPONENT_UECHO,
         ERR_NO_FREE_PACKET_BUFFER,
         (errorparameter_t)0,
         (errorparameter_t)0
      );
      openqueue_freePacketBuffer(reply); //clear the request packet as well
      return;
   }

   reply->owner                         = COMPONENT_UECHO;

   // reply with the same OpenQueueEntry_t
   reply->creator                       = COMPONENT_UECHO;
   reply->l4_protocol                   = IANA_UDP;
   reply->l4_destination_port           = 21568;
   reply->l4_sourcePortORicmpv6Type     = WKP_UDP_ECHO;
   reply->l3_destinationAdd.type        = ADDR_128B;

   // copy source to destination to echo.
   memcpy(&reply->l3_destinationAdd.addr_128b[0],&sid[0],16);

   packetfunctions_reserveHeaderSize(reply,27);


    struct timeval tv;
    gettimeofday(&tv,NULL);
    long long time_in_mill =  (((long long)tv.tv_sec)*1000)+(tv.tv_usec/1000);

   reply->payload[0] = 120;

   memset((char*)&reply->payload[1],0,27);
   int queue_size = openqueue_getNumEmpty();
   open_addr_t* addr = idmanager_getMyID(ADDR_16B);
   unsigned int l = ((int)addr->addr_16b[0]);
   unsigned int r = ((int)addr->addr_16b[1]);
   rrt_vars.counter++;

   snprintf((char*)&reply->payload[1],(size_t)27,"%lld;%u;%d;%d;%d",time_in_mill,queue_size,l,r,uecho_vars.counter);

   if ((openudp_send(reply))==E_FAIL) {
      openqueue_freePacketBuffer(reply);
   }


}
void uecho_sendDone(OpenQueueEntry_t* msg, owerror_t error) {
   openqueue_freePacketBuffer(msg);
}

bool uecho_debugPrint(void) {
   return FALSE;
}

//=========================== private =========================================
