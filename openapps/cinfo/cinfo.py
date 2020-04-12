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
for i in range(2, 6):
    if i != 10:
        id_ = "{}{}".format(MOTE_IP2, i)
    else:
        id_ = "{}{}".format(MOTE_IP2, "a")
    print id_
    c.POST('coap://[{0}]/rt'.format(id_),
          confirmable=False,
          # 	payload=[   	]
          )

while True:
    input = raw_input("Done. Press q to close. ")
    if input == 'q':
        print 'bye bye.'
        # c.close()
        os.kill(os.getpid(), signal.SIGTERM)
