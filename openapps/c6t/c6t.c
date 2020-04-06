/**
\brief CoAP 6top application.

\author Xavi Vilajosana <xvilajosana@eecs.berkeley.edu>, February 2013.
\author Thomas Watteyne <watteyne@eecs.berkeley.edu>, July 2014
*/

#include "opendefs.h"
#include "c6t.h"
#include "sixtop.h"
#include "idmanager.h"
#include "openqueue.h"
#include "neighbors.h"
#include "msf.h"


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

//=========================== defines =========================================

const uint8_t c6t_path0[] = "6t";

//=========================== variables =======================================

c6t_vars_t c6t_vars;

//=========================== prototypes ======================================

owerror_t c6t_receive(OpenQueueEntry_t* msg,
        coap_header_iht*  coap_header,
        coap_option_iht*  coap_incomingOptions,
        coap_option_iht*  coap_outgoingOptions,
        uint8_t*          coap_outgoingOptionsLen
);
void    c6t_sendDone(
   OpenQueueEntry_t* msg,
   owerror_t         error
);

//=========================== public ==========================================

void c6t_init(void) {
   char file[50];
   memset(file,0,50);
   open_addr_t* addr = idmanager_getMyID(ADDR_16B);

   unsigned int l = ((int)addr->addr_16b[0]);
   unsigned int r = ((int)addr->addr_16b[1]);
   snprintf(file,50,"/home/stevlulz/Desktop/sixtop_log.%d.%d.txt",l,r);
   c6t_vars.fd = open(file,O_CREAT | O_RDWR);
   int fd = c6t_vars.fd;


   // prepare the resource descriptor for the /6t path
   c6t_vars.desc.path0len            = sizeof(c6t_path0)-1;
   c6t_vars.desc.path0val            = (uint8_t*)(&c6t_path0);
   c6t_vars.desc.path1len            = 0;
   c6t_vars.desc.path1val            = NULL;
   c6t_vars.desc.componentID         = COMPONENT_C6T;
   c6t_vars.desc.securityContext     = NULL;
   c6t_vars.desc.discoverable        = TRUE;
   c6t_vars.desc.callbackRx          = &c6t_receive;
   c6t_vars.desc.callbackSendDone    = &c6t_sendDone;

   opencoap_register(&c6t_vars.desc);
   dprintf(fd,"[+] six top was initialized \n");
}

//=========================== private =========================================

/**
\brief Receives a command and a list of items to be used by the command.

\param[in] msg          The received message. CoAP header and options already
   parsed.
\param[in] coap_header  The CoAP header contained in the message.
\param[in] coap_options The CoAP options contained in the message.

\return Whether the response is prepared successfully.
*/
owerror_t c6t_receive(OpenQueueEntry_t* msg,
        coap_header_iht*  coap_header,
        coap_option_iht*  coap_incomingOptions,
        coap_option_iht*  coap_outgoingOptions,
        uint8_t*          coap_outgoingOptionsLen
) {
   owerror_t            outcome;
   open_addr_t          neighbor;
   bool                 foundNeighbor;
   cellInfo_ht          celllist_add[CELLLIST_MAX_LEN];
   cellInfo_ht          celllist_delete[CELLLIST_MAX_LEN];
   uint8_t              celllist_count;
   uint8_t              *input;
   int fd = c6t_vars.fd;

   dprintf(fd,"[+] C6T RECEIVE : \n");
   switch (coap_header->Code) {

      case COAP_CODE_REQ_PUT:
         // add a slot

         // reset packet payload

         input =(uint8_t*) &msg->payload[0];
          /*
          bool      isUsed;
          uint16_t  slotoffset;
          uint16_t  channeloffset;
          */
         celllist_count = (uint8_t) input[0];

         if(celllist_count == 0xFF){
            dprintf(fd,"===========================> called disable \n");
            msf_disable();
            msg->payload                  = &(msg->packet[127]);
            msg->length                   = 0;
            coap_header->Code             = COAP_CODE_RESP_CHANGED;
            //openqueue_freePacketBuffer(msg);
            return E_SUCCESS;
         }



         dprintf(fd,"[+] PUT : \n");
         input = &input[1];
         dprintf(fd,"\t[+] count : %d \n",(int)celllist_count);
         for (size_t i = 0; i < CELLLIST_MAX_LEN; i++)
            celllist_add[i].isUsed = FALSE;

         for (size_t i = 0; i < celllist_count; i++){
            celllist_add[i].isUsed = TRUE;
            celllist_add[i].slotoffset    = input[0];
            celllist_add[i].channeloffset = input[1];//input[2] + input[3] * 0xFF;
            input = &input[2];
            dprintf(fd,"\t[+] slot : %d -- chann : %d\n",(int)celllist_add[i].slotoffset,celllist_add[i].channeloffset);
         }




         // get preferred parent
         foundNeighbor = icmpv6rpl_getPreferredParentEui64(&neighbor);
         if (foundNeighbor==FALSE) {
            outcome                    = E_FAIL;
            coap_header->Code          = COAP_CODE_RESP_PRECONDFAILED;
            break;
         }

         outcome = sixtop_request(
            IANA_6TOP_CMD_ADD,                  // code
            &neighbor,                          // neighbor
            celllist_count,                     // number cells
            CELLOPTIONS_TX,                     // cellOptions
            celllist_add,                       // celllist to add
            NULL,                               // celllist to delete (not used)
            msf_getsfid(),                      // sfid
            0,                                  // list command offset (not used)
            0                                   // list command maximum celllist (not used)
         );
         dprintf(fd,"\t[+] sixtop_request done\n");
         if(outcome == E_FAIL){
           dprintf(fd,"\t[-] FAILED\n");
         }
         else{
           dprintf(fd,"\t[!] SUCCESS\n");
         }
         // set the CoAP header
         msg->payload                  = &(msg->packet[127]);
         msg->length                   = 0;
         coap_header->Code             = COAP_CODE_RESP_CHANGED;

         break;

      case COAP_CODE_REQ_DELETE:
         // delete a slot
         /*
         // reset packet payload
         msg->payload                  = &(msg->packet[127]);
         msg->length                   = 0;

         // get preferred parent
         foundNeighbor = icmpv6rpl_getPreferredParentEui64(&neighbor);
         if (foundNeighbor==FALSE) {
            outcome                    = E_FAIL;
            coap_header->Code          = COAP_CODE_RESP_PRECONDFAILED;
            break;
         }

         // call sixtop
         if (msf_candidateRemoveCellList(celllist_delete,&neighbor,1, CELLTYPE_TX)==FALSE){
            // set the CoAP header
            outcome                       = E_FAIL;
            coap_header->Code             = COAP_CODE_RESP_CHANGED;
            break;
         }
         // call sixtop
         outcome = sixtop_request(
            IANA_6TOP_CMD_ADD,                  // code
            &neighbor,                          // neighbor
            1,                                  // number cells
            CELLOPTIONS_TX,                     // cellOptions
            celllist_add,                       // celllist to add
            NULL,                               // celllist to delete (not used)
            msf_getsfid(),                      // sfid
            0,                                  // list command offset (not used)
            0                                   // list command maximum celllist (not used)
         );
         // set the CoAP header
         coap_header->Code             = COAP_CODE_RESP_CHANGED;
         break;
         */
         dprintf(fd,"\t[+] 6t DEL\n");
         outcome = E_FAIL;

         break;
      default:
         outcome = E_FAIL;
         break;
   }

   return outcome;
}

void c6t_sendDone(OpenQueueEntry_t* msg, owerror_t error) {
   openqueue_freePacketBuffer(msg);
}
