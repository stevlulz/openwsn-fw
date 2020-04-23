import time
import socket

NUM_TRIES = 10

request = "poipoipoipoi"
myAddress = ''  # means 'all'
myPort = 21568
hisPort = 7
delays = []
succ = 0
fail = 0
print "Testing udpEcho..."
MOTE_IP2 = 'bbbb::1415:92cc:0:'
f = open("try.txt", "a+")
# open socket
socket_handler = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
socket_handler.settimeout(5)
socket_handler.bind((myAddress, myPort))
for i in range(, 21):
    node_id_hex = "{}".format(hex(i))[2:]
    id_ = "{}{}".format(MOTE_IP2, node_id_hex)
    print id_
    socket_handler.sendto(request, (id_, hisPort))


while True:
    try:
        reply, dist_addr = socket_handler.recvfrom(1024)
        now = int(round(time.time() * 1000))
        string = "{};{}\n".format(now, reply)
        print "------> STRING : {}\n".format(string)
        f.write(string)

    except socket.timeout:
        # account
        fail += 1

        # log
        print "\nno reply"

    else:
        # account
        succ += 1

        # log
        output = []
        output += ['{0} ({1} bytes)'.format(reply, len(reply))]
        output = '\n'.join(output)
        print output

socket_handler.close()


