import sys
import time
import coap as coap
import matplotlib.pyplot as plt
import coapResource as Res
import networkx as nx

here = sys.path[0]
print (here)

topology = nx.DiGraph()
queue = {}
save_tab = {}


def init_save_tab():
    global topology
    global save_tab

    save_tab = {}
    m = max(topology.nodes)
    for k in range(1, m + 1):
        save_tab[k] = None


def init_queue(max_=20):
    for i in range(1, 13):
        queue[i] = max_ / 2
    queue[1] = 8


def display_queue():
    global queue
    print queue


def update_metric():
    global queue
    global topology
    ret_ = nx.Graph()
    for z in list(topology.edges.data()):
        if z[2]["parent"]:
            hum = queue[z[0]] * (200 - queue[z[1]])
            # if hum == 0:
            #    hum = -1000
            ret_.add_edge(z[0], z[1], weight=hum, parent=z[1], match=False)
    return ret_


def divide_node_with_color():
    global topology
    a = nx.coloring.greedy_color(topology)
    ret = {}
    for key in a:
        col_num = a[key]
        if col_num not in ret:
            ret[col_num] = [key]
        else:
            ret[col_num].append(key)
    return ret


def get_node_color(p_node, d):
    for key in d:
        if p_node in d[key]:
            return key
    return None


def get_children(Node, graph):
    res = []
    for child in list(graph.neighbors(Node)):
        if graph.edges[(Node, child)]["parent"] == Node:
            res.append(
                {
                    'node_id': child,
                    'weight': graph.edges[(Node, child)]["weight"]
                }
            )

    return res


# initial call with tree root
def max_weight_matching(Node, Tree):
    global save_tab
    # result was already calculated
    if save_tab[Node] is not None:
        return save_tab[Node]

    # get the set of Node's children
    children = get_children(Node, Tree)

    # if i do not have children == Node is leaf ==> cost = 0
    if len(children) == 0:
        save_tab[Node] = 0
        return 0

    sum1 = 0
    sum2 = 0
    chose = -1
    for elt in children:
        if save_tab[elt["node_id"]] is None:
            max_weight_matching(elt["node_id"], Tree)
        sum1 += save_tab[elt["node_id"]]

        sum_tmp = 0
        for ch_elt in get_children(elt["node_id"], Tree):
            if save_tab[ch_elt["node_id"]] is None:
                max_weight_matching(ch_elt["node_id"], Tree)
            sum_tmp += save_tab[ch_elt["node_id"]]
        sum_tmp += Tree.edges[(Node, elt["node_id"])]["weight"]
        sum_tmp -= save_tab[elt["node_id"]]
        if sum_tmp > sum2:
            chose = elt["node_id"]
        sum2 = max(sum2, sum_tmp)
    if sum1 < sum1 + sum2:
        if Tree.edges[(Node, chose)]["weight"] != 0:
            Tree.edges[(Node, chose)]["match"] = True
        for child in get_children(chose, Tree):
            Tree.edges[(child["node_id"], chose)]["match"] = False

    save_tab[Node] = max(sum1, sum1 + sum2)
    return sum1


def get_matching(Node, Tree):
    global save_tab
    res = []
    for i in range(1, 13):
        save_tab[i] = None
    max_weight_matching(Node, Tree)
    for e in Tree.edges.data():
        if e[2]["match"]:
            res.append((e[1], e[0],))
    return res, save_tab[1]


def calc_scheduler():
    init_save_tab()
    d = divide_node_with_color()
    init_queue(20)
    sched = {}
    print "Coloring : \n\t{}".format(d)
    start_time = time.time()
    for i in range(1, 120):
        ret = update_metric()

        sched[i] = {}
        res, cost = get_matching(1, ret)
        print (
            "Iter : {}\n\tCost : {} \n\tRes : \n\t\t{}\n\tQueue : {}\n\t{}\n".format(i, cost, res, queue,
                                                                                     ret.edges.data()))
        for elt in res:
            node_col = get_node_color(elt[0], d)
            if node_col not in sched[i]:
                sched[i][node_col] = []
            sched[i][node_col].append(elt[0])

            queue[elt[0]] -= 1
            if elt[1] != 1:
                queue[elt[1]] += 1
    print("--- {} seconds ---".format(time.time() - start_time))
    print "=============================================="

    print "Scheduler : \n"
    for key in sched:
        print "{} -> {}".format(key, sched[key])

    return sched


init_queue(20)
QUEUELENGTH = 20


def get_node_parent(num):
    global topology
    if num <= 1:
        return None
    a = list(topology.successors(num))
    for node in a:
        # print(t[num][node])
        if topology[num][node] and "parent" in topology[num][node] and topology[num][node]["parent"] == True:
            return node
    return None


def parser(payload, topo):
    print("[+] Payload parsing...!")
    global topology
    if payload[0] == 0x20:
        # announcement of one neighbor MESSAGE_TYPE 1
        print("[+] announcement of one neighbor MESSAGE_TYPE 1")
        if payload[6] == 0xFF and payload[7] == 0xFF:
            print("\t[+] GOOD FORMAT")
            from_ = payload[1] * 0xFF + payload[2]
            to_ = payload[3] * 0xFF + payload[4]
            rssi = payload[5]
            print("\t ADD {} --> {} : RSSI : {}".format(from_, to_, rssi))
            # tmp = topology.out_edges(from_)
            # topology.remove_edges_from(tmp)
            topology.add_edge(from_, to_, rssi=rssi, parent=1)
        else:
            print("\t[-] BAD FORMAT")
    elif payload[0] == 0x21:
        # [ 33   , 128,      2,            0,11,    0,7,    0,5,206, 0,7,206, 255]
        #  code   flags:p   neigh_count   from     parent  N1       N2       END
        # announcement of many neighbors MESSAGE_TYPE 1
        print("[+] announcement N neighbors")
        neighbor_count = payload[2]
        flags = payload[1]
        from_ = payload[3] * 0xFF + payload[4]
        parent = payload[5] * 0xFF + payload[6]
        print("\t ADD count = {}  -- from = {}  -- parent = {}   -- flags = {}".format(neighbor_count, from_, parent,
                                                                                       flags))
        i = 7
        for tmp in range(neighbor_count):
            to_ = payload[i] * 0xFF + payload[i + 1]
            rssi = payload[i + 2]
            print("\t\t {} --> {}  RSSI : {}".format(from_, to_, rssi))
            if to_ == parent:
                topology.add_edge(from_, to_, rssi=rssi, parent=1)
            else:
                topology.add_edge(from_, to_, rssi=rssi, parent=0)
            i = i + 3
    else:
        # not yet implemented
        print("[!] Code is not yet implemented")
    # parsing
    print("=====================================================================\n")


def my_callback(options, payload, topo):
    print("\n[+] Payload : \n{}\n".format(payload))
    print("Code {}".format(payload[0]))
    if payload[0] == 0x20 or payload[0] == 0x21:
        print("[+]Payload code was recognized")
        parser(payload, topo)
    else:
        print("[-] Payload code was not recognized")


# my ip addr bbbb::1/64
# or sudo ip -6 addr add bbbb::1415:92cc:ffff:1 dev tun0 to add new ip addr

# MOTE_IP = 'bbbb::1415:92cc:0:2'
UDPPORT = 5683  # can't be the port used in OV
tasa_res = Res.coapResource("tasa")
tasa_res.add_put_cb(my_callback)
tasa_res.add_topo(topology)
print("[+] resources created")
# sudo ip -6 addr add bbbb::1415:92cc:ffff:1 dev tun0

c = coap.coap(udpPort=UDPPORT, ipAddress='bbbb::1')
print("[+] coap server created")

c.addResource(tasa_res)
print("[+] resources added ")

# read status of debug LED
# p = c.GET('coap://[{0}]/l'.format(MOTE_IP))
# print chr(p[0])
#
# toggle debug LED
# p = c.PUT(
#    'coap://[{0}]/l'.format(MOTE_IP),
#    payload = [ord('2')],
# )
#
# read status of debug LED
# p = c.GET('coap://[{0}]/l'.format(MOTE_IP))
# print chr(p[0])
#
while True:
    user_input = raw_input(">>cmd : ")
    # print("Input : {}".format(input))
    if user_input == "help":
        print("""
    topo        --> display graphical plot of the current topology
    topo_text   --> display console-text view of current topology
    topo_color  --> calculate minimum number of colors for nodes
    calc_sched  --> re-launch scheduling computation
    queue       --> display queue\n
            """)
    elif user_input == "topo":
        plt.subplot(121)
        nx.draw(topology, with_labels=True, font_weight='bold')
        plt.show()
    elif user_input == "topo_txt":
        print ("Topology : ")
        print (list(topology.nodes))
        print (list(topology.edges))
    elif user_input == "queue_text":
        display_queue()
    elif user_input == "topo_color":
        print(divide_node_with_color())
    elif user_input == "calc_sched":
        calc_scheduler()
    elif user_input == "":
        continue
    else:
        print("Command is not valid, maybe you want try help cmd")
