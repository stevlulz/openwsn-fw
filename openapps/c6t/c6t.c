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
   c6t_vars.valid = 0;
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
   uint8_t              celllist_count;
   uint8_t              *input;
   int fd = c6t_vars.fd;
   dprintf(fd,"[+] C6T RECEIVE : \n");
   switch (coap_header->Code) {

      case COAP_CODE_REQ_PUT:
         // add a slot

         // reset packet payload

         input =(uint8_t*) &msg->payload[0];

         celllist_count = (uint8_t) input[0];
         input = &input[1];
         if(celllist_count == 0xFF){
            dprintf(fd,"===========================> called disable \n");
            msf_disable();
            msg->payload                  = &(msg->packet[127]);
            msg->length                   = 0;
            coap_header->Code             = COAP_CODE_RESP_CHANGED;
            //openqueue_freePacketBuffer(msg);
            return E_SUCCESS;
         }

         // get preferred parent
         foundNeighbor = icmpv6rpl_getPreferredParentEui64(&neighbor);
         if (foundNeighbor==FALSE) {
            outcome                    = E_FAIL;
            coap_header->Code          = COAP_CODE_RESP_PRECONDFAILED;
         }

        for (size_t i = 0; i < CELLLIST_MAX_LEN; i++){
            celllist_add[i].isUsed = FALSE;
            c6t_vars.celllist_delete[i].isUsed = FALSE;
        }
        size_t k =2;
        size_t next_add = 0;
        size_t next_delete = 0;
        size_t performed = 0;
        size_t tmp = celllist_count;
        slotinfo_element_t  info;
        c6t_vars.delete_count = 0;
        dprintf(fd,"[+] PUT : \n");
        dprintf(fd,"\t[+] count : %d \n",(int)celllist_count);
        int next_slot = 0;
        for(size_t l=0;l<celllist_count;l++){
            schedule_getSlotInfo(input[0],&info);
            if(info.address.type == ADDR_ANYCAST || info.link_type == CELLTYPE_TXRX){
                dprintf(fd,"\t\tslot :( %d ) ADDR_ANYCAST or CELLTYPE_TXRX\n",input[0]);
                input = &input[2];
                continue;
            }
            celllist_add[next_slot].isUsed = TRUE;
            celllist_add[next_slot].slotoffset    = input[0];
            celllist_add[next_slot].channeloffset = input[1];
            dprintf(fd,"\t\t[+] slot : %d  -- chan : %d\n",(int)celllist_add[next_slot].slotoffset,(int)celllist_add[next_slot].channeloffset);
            input = &input[2];
            next_slot++;
        }

        for(;k<102;k++){
            dprintf(fd,"[+] iteration %d : \n",k);
            schedule_getSlotInfo(k,&info);
            if(info.address.type == ADDR_ANYCAST || info.link_type == CELLTYPE_TXRX){
                dprintf(fd,"\t\tADDR_ANYCAST or CELLTYPE_TXRX\n");
                if (input[0] == info.slotOffset){
                    input = &input[2];
                    tmp--;
                }
                continue;
            }
            if(info.link_type == CELLTYPE_TX && info.isAutoCell == FALSE ){

                c6t_vars.celllist_delete[next_delete].isUsed = TRUE;
                c6t_vars.celllist_delete[next_delete].slotoffset = info.slotOffset;
                c6t_vars.celllist_delete[next_delete].channeloffset = info.channelOffset;
                dprintf(fd,"\t\t[-] slot : %d -- chann : %d\n",
                (int)c6t_vars.celllist_delete[next_delete].slotoffset,
                c6t_vars.celllist_delete[next_delete].channeloffset
                );
                next_delete++;
                c6t_vars.delete_count++;
            }
        }
        //add new schedule

        dprintf(fd,"TEST Added : %d -- Deleted : %d\n",next_add,next_delete);
        for(int z=0;z<next_delete;z++){
            dprintf(fd,"\t\tDELETE slot : %d -- chann : %d     %d\n",
            (int)c6t_vars.celllist_delete[z].slotoffset,
            (int)c6t_vars.celllist_delete[z].channeloffset,
            (int)c6t_vars.celllist_delete[z].isUsed);
        }
        for(int z=0;z<next_slot;z++){
            dprintf(fd,"\t\tADD slot : %d -- chann : %d     %d\n",
            (int)celllist_add[z].slotoffset,
            (int)celllist_add[z].channeloffset,
            (int)celllist_add[z].isUsed);
        }
         outcome = sixtop_request(
            IANA_6TOP_CMD_ADD,                  // code
            &neighbor,                          // neighbor
            next_slot,                     // number cells
            CELLOPTIONS_TX,                     // cellOptions
            celllist_add,                       // celllist to add
            NULL,                               // celllist to delete (not used)
            msf_getsfid(),                      // sfid
            0,                                  // list command offset (not used)
            0                                   // list command maximum celllist (not used)
         );


         msg->payload                  = &(msg->packet[127]);
         msg->length                   = 0;
         coap_header->Code             = COAP_CODE_RESP_CHANGED;



        //DONE
         dprintf(fd,"\t[+] sixtop_request done\n");
         if(outcome == E_FAIL){
           dprintf(fd,"\t[-] FAILED\n");
         }
         else{
           dprintf(fd,"\t[!] SUCCESS\n");
         }

         break;

      case COAP_CODE_REQ_DELETE:
         // delete a slot

        // reset packet payload
        msg->payload                  = &(msg->packet[127]);
        msg->length                   = 0;
        dprintf(fd,"COAP DELETE \n");
        // get preferred parent
        foundNeighbor = icmpv6rpl_getPreferredParentEui64(&neighbor);
        if (foundNeighbor==FALSE) {
            outcome                    = E_FAIL;
            coap_header->Code          = COAP_CODE_RESP_PRECONDFAILED;
            break;
        }
        dprintf(fd,"COAP DELETE FOUND PARENT\n");
        dprintf(fd,"DELETE COUNT %d\n",c6t_vars.delete_count);
        outcome = sixtop_request(
            IANA_6TOP_CMD_DELETE,                  // code
            &neighbor,                          // neighbor
            c6t_vars.delete_count,                     // number cells
            CELLOPTIONS_RX,                     // cellOptions
            NULL,                       // celllist to add
            c6t_vars.celllist_delete,                               // celllist to delete (not used)
            msf_getsfid(),                      // sfid
            0,                                  // list command offset (not used)
            0                                   // list command maximum celllist (not used)
        );
        if(outcome == E_FAIL){
            dprintf(fd,"\t[-] DELETE FAILED\n");
        }
        else{
            dprintf(fd,"\t[!] DELETE SUCCESS\n");
        }

        coap_header->Code             = COAP_CODE_RESP_DELETED;

        dprintf(fd,"\t[+] 6t DEL\n");
        //outcome = E_FAIL;

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
