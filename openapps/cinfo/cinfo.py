import os
import sys

here = sys.path[0]
print here
sys.path.insert(0, os.path.join(here, '..', '..', '..', 'coap'))

from coap import coap
import signal

MOTE_IP2 = 'bbbb::1415:92cc:0:'

UDPPORT = 61618  # can't be the port used in OV
id_ = ""
c = coap.coap(udpPort=UDPPORT)

for i in range(3, 4):

    node_id_hex = "{}".format(hex(i))[2:]
    id_ = "{}{}".format(MOTE_IP2, node_id_hex)
    print id_
    try:
        c.POST('coap://[{0}]/rt'.format(id_),
               confirmable=False,
               # 	payload=[   	]
               )
    except:
        print "didn't receive ACK rrt"
while True:
    input = raw_input("Done. Press q to close. ")
    if input == 'q':
        print 'bye bye.'
        # c.close()
        os.kill(os.getpid(), signal.SIGTERM)

"""
after having the scheduling setup for all nodes
we shall start rrt application in all nodes
this application is a simple coap app that sends every second its identifier,number of empty places in its queue,and finally the time of send

in sch.py end it will be receiving these packets, upon receiving , it will get current time, and simply put it in a file
this data will be used for evaluation later with msf ; queue level evaluation, end_to_end delay ^^

by default rrt applications are disabled, i'm enabling them manually cuz i've send scheduling table to all nodes
so up on receiving they will wait 100sec then start sending packets  every 1 second!!

normally in some seconds, we will be receiving these packets from all nodes!
"""