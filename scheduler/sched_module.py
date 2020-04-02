import networkx as nx
import time

topology = nx.DiGraph()
queue = {}


def generate_mytopo():
    global topology
    topology.add_edge(1, 2, parent=False)

    topology.add_edge(2, 1, parent=True)
    topology.add_edge(2, 7, parent=False)
    topology.add_edge(2, 5, parent=False)

    topology.add_edge(3, 5, parent=True)

    topology.add_edge(4, 9, parent=False)
    topology.add_edge(4, 10, parent=True)

    topology.add_edge(5, 2, parent=True)
    topology.add_edge(5, 3, parent=False)
    topology.add_edge(5, 9, parent=False)
    topology.add_edge(5, 10, parent=False)
    topology.add_edge(5, 11, parent=False)

    topology.add_edge(6, 9, parent=True)

    topology.add_edge(7, 2, parent=True)
    topology.add_edge(7, 8, parent=False)
    topology.add_edge(7, 11, parent=False)
    topology.add_edge(7, 12, parent=False)

    topology.add_edge(8, 7, parent=False)
    topology.add_edge(8, 11, parent=True)
    topology.add_edge(8, 12, parent=False)

    topology.add_edge(9, 4, parent=False)
    topology.add_edge(9, 5, parent=False)
    topology.add_edge(9, 6, parent=False)
    topology.add_edge(9, 11, parent=True)

    topology.add_edge(10, 4, parent=False)
    topology.add_edge(10, 5, parent=True)

    topology.add_edge(11, 5, parent=False)
    topology.add_edge(11, 7, parent=True)
    topology.add_edge(11, 8, parent=False)
    topology.add_edge(11, 9, parent=False)

    topology.add_edge(12, 7, parent=True)
    topology.add_edge(12, 8, parent=False)


def init_queue(max_=200):
    for i in range(1, 13):
        queue[i] = max_ / 2
    queue[1] = 8


init_queue()


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
        # print z

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




d = divide_node_with_color()


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


save_tab = {}


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
            node_col = get_node_color(elt[0])
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
