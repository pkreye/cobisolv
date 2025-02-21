import numpy as np
from random import randint, seed, shuffle, choice, choices, sample
from networkx import Graph, bfs_edges,single_source_shortest_path_length,connected_components, minimum_spanning_tree
#from copy import deepcopy
from collections import defaultdict
from dimod.utilities import qubo_to_ising
from time import time
from math import ceil, log2
from collections import deque
from itertools import groupby
import cobi


# input_dir = "./QUICC_Datasets_3SAT/batch-03"
# filename = "3block-like-rand-00000-0027"
input_dir = "../data/N_eq_50_seen/"
filename = "01"
extension = '.cnf'
LOG_PROBS = True
#input_dir = "/home/chriskim00/efe00002/cobisolv/isd/QUICC_Datasets_3SAT/batch-06"
#filename = "stat-hard-4SAT-r-3-024781-0082"
#filename = 'stat-hard-6SAT-r-4-0000'
#filename = 'stat-hard-4SAT-r-3-003397-0033'
#extension = '.cnf'
#device_lock = None

NUM_ITERS = 100
SOLVER_SPINS = 45
HWDEBUG = 0
LF_SIZE = 1
SCALE = 4
#DEVICE = 1
# 0 for "/dev/cobi_pcie_card0"
# 1 for "/dev/cobi_pcie_card1"
# 2 for "/dev/cobi_pcie_card2"
# 3 for "/dev/cobi_pcie_card3"
# 4 for "/dev/cobi_pcie_card4"

NEIGHBOR_SUFFLE      = True
LOCAL_SEARCH         = True
LOCAL_SEARCH_LIMIT   = 1000
NO_SAME_ROOT_TWICE   = False

# select the root from highest connected ones
GREEDY_ROOT          = False

# Select the root from unselected variables
GREEDY_RANDOM_ROOT   = False

REDUCE_NEIGHBORS     = False

# pick the high weight nodes
LIMITED_NEIGHBORS         = True

# pick the low weight nodes
REVERSE_LIMITED_NEIGHBORS = False

#NEIGHBOR_LIMIT            = 10

WEIGHTED_BFS         = False
WEIGHTED_BFSV2       = False
WEIGHTED_SHUFFLE_BFS = False

ILP_COMB             = False
FLAT_ILP_COMB        = False

SEEK_INITIAL         = False
SIGNED_EDGES         = False

############## ILP FUNCTIONS ##############
def addClause(mat, clause, numvars, slackVarIndex):
    if len(clause) == 1:
        addVal(mat, (abs(clause[0])-1, abs(clause[0])-1), (1 if clause[0] < 0 else -1))
        return slackVarIndex
    numSlackBits = ceil(log2(len(clause)))
    coeff = {-1:-1} # constant term
    for c in clause:
        if c < 0:
            coeff[abs(c) - 1] = -1
            coeff[-1] += 1
        else:
            coeff[c - 1] = 1
    for i in range(numSlackBits):
        coeff[slackVarIndex + i] = -2 ** i

    tmpmat, tmpconst = squareTerm(coeff)
    for (ind, val) in tmpmat.items():
      addVal(mat, ind, val)
    return slackVarIndex + numSlackBits

def addClauseV2(mat, clause, numvars, slackVarIndex):
    if len(clause) == 1:
        addVal(mat, (abs(clause[0])-1, abs(clause[0])-1), (1 if clause[0] < 0 else -1))
        return slackVarIndex
    numSlackBits = len(clause)-1
    coeff = {-1:-1} # constant term
    for c in clause:
        if c < 0:
            coeff[abs(c) - 1] = -1
            coeff[-1] += 1
        else:
            coeff[c - 1] = 1
    for i in range(numSlackBits):
        coeff[slackVarIndex + i] = -1

    tmpmat, tmpconst = squareTerm(coeff)
    for (ind, val) in tmpmat.items():
      addVal(mat, ind, val)
    return slackVarIndex + numSlackBits

def squareTerm(coeff):
    tmpmat = dict()
    const = 0
    for (ind, val1) in coeff.items():
        for (jnd, val2) in coeff.items():
            if ind != -1 and jnd != -1:
                addVal(tmpmat, (min(ind, jnd), max(ind, jnd)), (val1 * val2))
            elif ind != -1 and jnd == -1:
                addVal(tmpmat, (ind, ind), (val1 * val2))
            elif jnd != -1 and ind == -1:
                addVal(tmpmat, (jnd, jnd), (val1 * val2))
            else:
                const += (val1 * val2)
    return tmpmat, const

def addVal(mat, ind, val):
    ind = (min(ind[0], ind[1]), max(ind[0], ind[1]))
    mat.setdefault(ind, 0)
    mat[ind] += val

###########################################

def estimate_ancillaries(c):
    anc_count = 0 # 1sat or 2sat
    if ILP_COMB:
        if(len(c)==3):
            anc_count = 1
        elif(len(c)>=4):
            anc_count = ceil(log2(len(c)))
    elif FLAT_ILP_COMB:
        if(len(c)==3):
            anc_count = 1
        elif(len(c)>=4):
            anc_count = len(c)-1
    else:
        anc_count = ancillary_counter(c) #ancillaries
    return anc_count

def report_globals():
    # Get the dictionary of the current global symbol table
    global_vars = globals()

    # Identify user-defined variables (exclude built-in and imported ones)
    user_defined_globals = {k: v for k, v in global_vars.items() if not k.startswith("__") and not callable(v) and not k in dir(__builtins__)}

    return user_defined_globals

def cobi_sample(Q_orig, surrogate_vars, HWDEBUG):
    variables = [x-1 for x in surrogate_vars] #indexing bug fix
#    variables = []
    for (i,j),w in Q_orig.items():
        if w != 0:
            if i not in variables:
                variables.append(i)
            if j not in variables:
                variables.append(j)
    variables.sort()

    Q_init = {}
    for (i,j),w in Q_orig.items():
        Q_init[(variables.index(i)+1, variables.index(j)+1)] = w
    h,J,offset = qubo_to_ising(Q_init)
    for h_i in h:
        J[(0,h_i)] = h[h_i]
    MTXSIZE = 46
    mtx = np.zeros([MTXSIZE, MTXSIZE], dtype = int)
    clamped = 0
    for ix, iy in np.ndindex(mtx.shape):
        if (ix,iy) in J.keys():
            if abs((SCALE*J[ix, iy])) > 7:
                J[ix, iy] = np.sign(J[ix, iy]) * 7
                clamped += 1
            else:
                J[ix, iy] = (SCALE*J[ix, iy])
            mtx[ix][iy] = mtx[iy][ix] = -J[ix, iy]

    ising = {'problem': [[int(0)] * 46] * 46}
    ising['problem'] = mtx.tolist()

    # print(ising['problem'])
    # exit(1)

    # result = {'problem_id': 0, 'energy': 0, 'spins': [0] * 46, 'core_id':0}

    # w_submit_problem(DEVICE, ising)
    # result = w_read_result_blocking(DEVICE)
    # result = w_solveQ(ising,result,DEVICE)

    tic = time()
    result = cobi.run_native_problem(ising)
    toc = time()
    cobi_time = toc - tic

    # print(result)

    # LFO spin is not returned by cobiserv wrapper, for now extend array with a leading 1
    best_sample = [1] + result["spins"]

    # ising = {'Q': [[int(0)] * 46] * 46, 'energy': 0, 'spins': [0] * 46, 'debug': HWDEBUG}
    # ising['Q'] = mtx.tolist()
    if LOG_PROBS:
        with open('prob.log', 'a') as problog:
            for row in ising['problem']:
                problog.write(str(row))
            problog.write('\n==\n')
    # #print(ising)
    # #exit()
    # tic = time()
    # if device_lock == None:
    #     lock_waiting_time = 0
    #     #print('No device lock')
    #     tic = time()
    #     ising = w_solveQ(ising,DEVICE)
    #     toc = time()
    #     PCIE_time = toc - tic
    # else:
    #     #print('Using the device lock')
    #     with device_lock:
    #         toc = time()
    #         lock_waiting_time = toc-tic
    #         tic = time()
    #         ising = w_solveQ(ising,DEVICE)
    #         toc = time()
    #         PCIE_time = toc - tic
    # best_sample = ising["spins"]
    # best_sample.reverse()
    state_sample = {k_i:best_sample[k_i] for k_i in range(len(variables)+1)}

    state_sample_int = {}
    for key in state_sample:
        if key != 0:
            if state_sample[0]==1:
                state_sample_int[int(key)] = 1 if int(state_sample[key])==1 else 0
            else:
                state_sample_int[int(key)] = 0 if int(state_sample[key])==1 else 1
    best_sample_binary = list(state_sample_int.values())

    samples = {}
    for v in range(len(variables)):
        samples[variables[v]] = best_sample_binary[v]

    return samples, cobi_time, clamped

def reduce_neighbors(G):
    G_remaining = G.copy()
    starting_partitions  = len(list(connected_components(G)))
    # Sort edges by weight (ascending)
    # Step 1: Sort edges by weight
    edges_by_weight = sorted(G_remaining.edges(data=True), key=lambda x: x[2]['weight'])

    # Step 2: Group edges with the same weight and shuffle each group
    shuffled_edges = []
    for weight, edges in groupby(edges_by_weight, key=lambda x: x[2]['weight']):
        edge_list = list(edges)
        shuffle(edge_list)  # Shuffle edges with the same weight
        shuffled_edges.extend(edge_list)  # Add to final list
    # Remove lowest energy edge
    for edge in shuffled_edges:
        if(len(list(connected_components(G_remaining))) == starting_partitions):
            #print(f'Removing edge {edge[0]}-{edge[1]}')
            removed_edge = edge
            removed_edge_data = G.get_edge_data(edge[0], edge[1])
            G_remaining.remove_edge(edge[0], edge[1])
        else: # add the removed edge back
            #print(f'Adding edge {removed_edge[0]}-{removed_edge[1]}')
            G_remaining.add_edge(removed_edge[0], removed_edge[1], **removed_edge_data)
            return G_remaining
    return G

def limited_neighbors(graph, k):
    """
    Simplify a graph to retain exactly `k` connections per node
    while ensuring the graph remains connected.

    Parameters:
    - graph (nx.Graph): The input graph.
    - k (int): The number of connections to retain per node.

    Returns:
    - nx.Graph: The simplified graph.
    """
    original_weights = {(u, v): data['weight'] for u, v, data in graph.edges(data=True)}

    # Step 2: Invert the weights for maximum spanning tree
    for u, v, d in graph.edges(data=True):
        if REVERSE_LIMITED_NEIGHBORS:
            d['weight'] = abs(d['weight'])
        else:
            d['weight'] = -abs(d['weight'])  # Invert the weights for maximum spanning tree

    # Step 3: Create the Maximum Spanning Tree (MST)
    mst = minimum_spanning_tree(graph)

    # Step 4: Restore original weights in the MST
    for u, v, d in mst.edges(data=True):
        d['weight'] = original_weights.get((u, v), None)  # Restore original weight if it exists

    # Step 5: Restore original weights in the original graph
    for u, v, d in graph.edges(data=True):
        d['weight'] = original_weights.get((u, v), None)  # Restore original weight if it exists


    # Step 2: Add edges to ensure `k` connections per node
    H = mst.copy()
    for node in graph.nodes:
        # Get existing neighbors and their edge weights
        existing_neighbors = set(H.neighbors(node))
        needed_connections = k - len(existing_neighbors)

        if needed_connections > 0:
            # Get all remaining edges connected to the node, sorted by weight
            if REVERSE_LIMITED_NEIGHBORS:
                candidate_edges = sorted(
                    ((node, neighbor, data) for neighbor, data in graph[node].items() if neighbor not in existing_neighbors),
                    key=lambda x: abs(x[2]['weight']),
                    reverse=False,
                )
            else:
                candidate_edges = sorted(
                    ((node, neighbor, data) for neighbor, data in graph[node].items() if neighbor not in existing_neighbors),
                    key=lambda x: abs(x[2]['weight']),
                    reverse=True,
                )

            # Add the top edges to satisfy the `k`-connection requirement
            for u, v, d in candidate_edges[:needed_connections]:
                H.add_edge(u, v, **d)
    return H

def weighted_bfs(G, root):
    visited = set()
    queue = deque([root])
    visited.add(root)
    result_edges = []
    nodes = [root]

    while queue:
        current_node = queue.popleft()

        # Get unvisited neighbors and their weights
        neighbors = [
            (neighbor, G[current_node][neighbor].get('weight', 1))
            for neighbor in G.neighbors(current_node)
            if neighbor not in visited
        ]

        # Sort neighbors by weight in descending order
        neighbors.sort(key=lambda x: x[1], reverse=True)

        # Visit sorted neighbors
        for neighbor, weight in neighbors:
            if neighbor not in visited:
                visited.add(neighbor)
                queue.append(neighbor)
                result_edges.append((current_node, neighbor))  # Record the edge in BFS order
                nodes.append(neighbor)

    return result_edges, nodes

def weighted_bfsV2(G, root, max_nodes=None):
    visited = set()
    queue = deque([(root, 0)])  # Store nodes along with their depth (0 is the root's depth)
    visited.add(root)
    result_edges = []
    nodes = [root]
    #print('Max nodes = ', max_nodes)

    while queue:
        current_node, current_depth = queue.popleft()

        # If max_nodes is given and we have already visited the max number of nodes, stop.
        if max_nodes is not None and len(nodes) >= max_nodes:
            break

        # Get unvisited neighbors and their weights
        neighbors = [
            (neighbor, G[current_node][neighbor].get('weight', 1))
            for neighbor in G.neighbors(current_node)
            if neighbor not in visited
        ]

        # Sort neighbors by weight in descending order
        neighbors.sort(key=lambda x: x[1], reverse=True)

        # Group neighbors by weight
        grouped_neighbors = groupby(neighbors, key=lambda x: x[1])

        # Visit each group of neighbors, shuffling those with the same weight
        flat_neighbors = []
        for weight, group in grouped_neighbors:
            group_list = list(group)  # Convert the group to a list
            shuffle(group_list)  # Shuffle the neighbors with the same weight
            flat_neighbors.extend(group_list)  # Flatten the shuffled group into the list

        # Visit neighbors in the final order (sorted and shuffled)
        for neighbor, weight in flat_neighbors:
            if neighbor not in visited:
                visited.add(neighbor)
                queue.append((neighbor, current_depth + 1))  # Increment depth for the next level
                result_edges.append((current_node, neighbor))  # Record the edge in BFS order
                nodes.append(neighbor)
                # Stop if we've visited the maximum number of nodes
                if max_nodes is not None and len(nodes) >= max_nodes:
                    break

    return result_edges, nodes

def weighted_bfs_with_shuffle(G, root):
    visited = set()
    queue = deque([root])
    visited.add(root)
    result_edges = []
    nodes = [root]

    while queue:
        current_node = queue.popleft()

        # Get unvisited neighbors and their weights
        neighbors = [
            (neighbor, G[current_node][neighbor].get('weight', 1))
            for neighbor in G.neighbors(current_node)
            if neighbor not in visited
        ]

        # Randomly shuffle neighbors by weights
        if neighbors:
            neighbors = choices(neighbors, weights=[weight for _, weight in neighbors], k=len(neighbors))

        # Visit shuffled neighbors
        for neighbor, weight in neighbors:
            if neighbor not in visited:
                visited.add(neighbor)
                queue.append(neighbor)
                result_edges.append((current_node, neighbor))  # Record the edge in BFS order
                nodes.append(neighbor)

    return result_edges, nodes

def import_cnf(input_dir,filename):
    my_file = open(input_dir + "/" + filename, "r")
    data = my_file.read()
    in_data = data.split("\n")

    cnf = list()
    cnf.append(list())
    maxvar = 0
    ksat = 0

    for line in in_data:
        tokens = line.split()
        if len(tokens) != 0 and tokens[0] not in ("p", "c", "%", "0"):
            for tok in tokens:
                lit = int(tok)
                maxvar = max(maxvar, abs(lit))
                if lit == 0:
                    if len(cnf[-1]) > ksat:
                        ksat = len(cnf[-1])
                    cnf.append(list())
                else:
                    cnf[-1].append(lit)
    assert len(cnf[-1]) == 0
    cnf.pop()
    return cnf, maxvar, ksat

def get_idt(cnf, maxvar):
    Q = defaultdict(int)
    for i in range(maxvar):
        Q[(i,i)] = 0

    ancillary_index = maxvar
    for c in cnf:
        if len(c) == 3:
            ecoeffs = np.zeros(len(c))
            wcoeff = 0

            # (1) * (x0+x1+x2)
            for lit in c:
                Q[(abs(lit)-1,abs(lit)-1)] += np.sign(lit)
                wcoeff += np.sign(lit) < 0
                for i,lit2 in enumerate(c):
                    ecoeffs[i] -= np.sign(lit2) if ((lit != lit2) and (np.sign(lit) < 0)) else 0

            # negation coeffs from -x0x1-x0x2-x1x2
            for i,lit in enumerate(c):
                Q[(abs(lit)-1,abs(lit)-1)] += int(ecoeffs[i])

            # (w) * (x0+x1+x2)
            for lit in c:
                Q[tuple(sorted([abs(lit)-1,ancillary_index]))] = np.sign(lit)

            # -x0x1-x0x2-x1x2
            signs = [np.sign(c[0])*np.sign(c[1]),
                    np.sign(c[0])*np.sign(c[2]),
                    np.sign(c[1])*np.sign(c[2])]
            pairs = [tuple(sorted([abs(c[0])-1,abs(c[1])-1])),
                    tuple(sorted([abs(c[0])-1,abs(c[2])-1])),
                    tuple(sorted([abs(c[1])-1,abs(c[2])-1]))]
            for i,p in enumerate(pairs):
                if p in Q:
                    Q[p] += - signs[i]
                else:
                    Q[p] = - signs[i]

            # -2w
            Q[ancillary_index,ancillary_index] = -2 + wcoeff

            ancillary_index += 1
        elif len(c) == 2:
            if c[0] < 0 and c[1] < 0:
                Q[tuple(sorted([abs(c[0])-1, abs(c[1])-1]))] -= 1
            elif c[0] < 0 and c[1] > 0:
                Q[tuple(sorted([abs(c[0])-1, abs(c[0])-1]))] -= 1
                Q[tuple(sorted([abs(c[0])-1, abs(c[1])-1]))] += 1
            elif c[0] > 0 and c[1] < 0:
                Q[tuple(sorted([abs(c[1])-1, abs(c[1])-1]))] -= 1
                Q[tuple(sorted([abs(c[0])-1, abs(c[1])-1]))] += 1
            elif c[0] > 0 and c[1] > 0:
                Q[tuple(sorted([abs(c[0])-1, abs(c[0])-1]))] += 1
                Q[tuple(sorted([abs(c[1])-1, abs(c[1])-1]))] += 1
                Q[tuple(sorted([abs(c[0])-1, abs(c[1])-1]))] -= 1
        elif len(c) == 1:
            Q[(abs(c[0])-1,abs(c[0])-1)] += 1 if c[0] > 0 else -1
        else:
            ValueError('k>3 in provided SAT instance')


    Q = {key:-Q[key] for key in Q}
    return Q

def get_idt2(cnf, maxvar):
    Q = defaultdict(int)
    for i in range(maxvar):
        Q[(i,i)] = 0

    ancillary_index = maxvar
    for c in cnf:
        if len(c) == 3:
            ecoeffs = np.zeros(len(c))
            wcoeff = 0

            # (1) * (x0+x1+x2)
            for lit in c:
                Q[(abs(lit)-1,abs(lit)-1)] += np.sign(lit)
                wcoeff += np.sign(lit) < 0
                for i,lit2 in enumerate(c):
                    ecoeffs[i] -= np.sign(lit2) if ((lit != lit2) and (np.sign(lit) < 0)) else 0

            # negation coeffs from -x0x1-x0x2-x1x2
            for i,lit in enumerate(c):
                Q[(abs(lit)-1,abs(lit)-1)] += int(ecoeffs[i])

            # (w) * (x0+x1+x2)
            for lit in c:
                Q[tuple(sorted([abs(lit)-1,ancillary_index]))] = np.sign(lit)

            # -x0x1-x0x2-x1x2
            signs = [np.sign(c[0])*np.sign(c[1]),
                    np.sign(c[0])*np.sign(c[2]),
                    np.sign(c[1])*np.sign(c[2])]
            pairs = [tuple(sorted([abs(c[0])-1,abs(c[1])-1])),
                    tuple(sorted([abs(c[0])-1,abs(c[2])-1])),
                    tuple(sorted([abs(c[1])-1,abs(c[2])-1]))]
            for i,p in enumerate(pairs):
                if p in Q:
                    Q[p] += - signs[i]
                else:
                    Q[p] = - signs[i]

            # -2w
            Q[ancillary_index,ancillary_index] = -2 + wcoeff

            ancillary_index += 1
        elif len(c) == 2:
            if c[0] < 0 and c[1] < 0:
                Q[tuple(sorted([abs(c[0])-1, abs(c[1])-1]))] -= 1
            elif c[0] < 0 and c[1] > 0:
                Q[tuple(sorted([abs(c[0])-1, abs(c[0])-1]))] -= 1
                Q[tuple(sorted([abs(c[0])-1, abs(c[1])-1]))] += 1
            elif c[0] > 0 and c[1] < 0:
                Q[tuple(sorted([abs(c[1])-1, abs(c[1])-1]))] -= 1
                Q[tuple(sorted([abs(c[0])-1, abs(c[1])-1]))] += 1
            elif c[0] > 0 and c[1] > 0:
                Q[tuple(sorted([abs(c[0])-1, abs(c[0])-1]))] += 1
                Q[tuple(sorted([abs(c[1])-1, abs(c[1])-1]))] += 1
                Q[tuple(sorted([abs(c[0])-1, abs(c[1])-1]))] -= 1
        elif len(c) == 1:
            Q[(abs(c[0])-1,abs(c[0])-1)] += 1 if c[0] > 0 else -1

    Q = {key:-Q[key] for key in Q}
    return Q,ancillary_index

def calculate_varcnt(cnf):
    bool_vars = list()
    anc_count = 0
    for c in cnf:
        for l in c:
            if abs(l) not in bool_vars:
                bool_vars.append(abs(l))
        anc_count += ancillary_counter(c) #ancillaries
    return (len(bool_vars) + anc_count), bool_vars

def calculate_varcnt2(cnf):
    bool_vars = set()
    anc_count = 0
    for c in cnf:
        bool_vars.update(set(map(abs, c)))
        anc_count += estimate_ancillaries(c)
    return (len(bool_vars) + anc_count), bool_vars

def ancillary_counter(clause):
    clause_length = len(clause)
    if clause_length <= 2:
        return 0
    return (clause_length - 2) * 2 - 1


def sat_kN_to_k3_V2(formula,V):
    f2,v = [],V
    for f in formula:
        if len(f) <= 3:
            f2.append(f)
        elif len(f) == 4:
            f2.append([f[0],f[1],v])
            f2.append([f[2],f[3],-v])
            v += 1
        elif len(f) == 5:
            f2.append([f[0],f[1],-v])
            f2.append([v,f[2],-(v+1)])
            f2.append([v+1,f[3],f[4]])
            v += 2
        elif len(f) == 6:
            f2.append([f[0],f[1],-v])
            f2.append([v,f[2],-(v+1)])
            f2.append([v+1,f[3],-(v+2)])
            f2.append([v+2,f[4],f[5]])
            v += 3
        elif len(f) == 7:
            f2.append([f[0],f[1],-v])
            f2.append([v,f[2],-(v+1)])
            f2.append([v+1,f[3],-(v+2)])
            f2.append([v+2,f[4],-(v+3)])
            f2.append([v+3,f[5],f[6]])
            v += 4
        elif len(f) == 8:
            f2.append([f[0],f[1],-v])
            f2.append([v,f[2],-(v+1)])
            f2.append([v+1,f[3],-(v+2)])
            f2.append([v+2,f[4],-(v+3)])
            f2.append([v+3,f[5],-(v+4)])
            f2.append([v+4,f[6],f[7]])
            v += 5
        elif len(f) == 9:
            f2.append([f[0],f[1],-v])
            f2.append([v,f[2],-(v+1)])
            f2.append([v+1,f[3],-(v+2)])
            f2.append([v+2,f[4],-(v+3)])
            f2.append([v+3,f[5],-(v+4)])
            f2.append([v+4,f[6],-(v+5)])
            f2.append([v+5,f[7],f[8]])
            v += 6
        elif len(f) == 10:
            f2.append([f[0],f[1],-v])
            f2.append([v,f[2],-(v+1)])
            f2.append([v+1,f[3],-(v+2)])
            f2.append([v+2,f[4],-(v+3)])
            f2.append([v+3,f[5],-(v+4)])
            f2.append([v+4,f[6],-(v+5)])
            f2.append([v+5,f[7],-(v+6)])
            f2.append([v+6,f[8],f[9]])
            v += 7
        elif len(f) == 11:
            f2.append([f[0],f[1],-v])
            f2.append([v,f[2],-(v+1)])
            f2.append([v+1,f[3],-(v+2)])
            f2.append([v+2,f[4],-(v+3)])
            f2.append([v+3,f[5],-(v+4)])
            f2.append([v+4,f[6],-(v+5)])
            f2.append([v+5,f[7],-(v+6)])
            f2.append([v+6,f[8],-(v+7)])
            f2.append([v+7,f[9],f[10]])
            v += 8
        elif len(f) == 12:
            f2.append([f[0],f[1],-v])
            f2.append([v,f[2],-(v+1)])
            f2.append([v+1,f[3],-(v+2)])
            f2.append([v+2,f[4],-(v+3)])
            f2.append([v+3,f[5],-(v+4)])
            f2.append([v+4,f[6],-(v+5)])
            f2.append([v+5,f[7],-(v+6)])
            f2.append([v+6,f[8],-(v+7)])
            f2.append([v+7,f[9],-(v+8)])
            f2.append([v+8,f[10],f[11]])
            v += 9
        else:
            raise(NotImplementedError)
    return f2, v

def sat_kN_to_k3(formula,V):
    f2,v = [],V
    for f in formula:
        if len(f) <= 3:
            f2.append(f)
        elif len(f) == 4:
            f2.append([f[0],f[1],v])
            f2.append([f[2],f[3],-v])
            v += 1
        elif len(f) == 5:
            f2.append([f[0],f[1],-v])
            f2.append([f[2],f[3],-(v+1)])
            f2.append([v,v+1,f[4]])
            v += 2
        elif len(f) == 6:
            f2.append([f[0],f[1],-v])
            f2.append([f[2],f[3],-(v+1)])
            f2.append([f[4],f[5],-(v+2)])
            f2.append([v,v+1,v+2])
            v += 3
        elif len(f) == 7:
            f2.append([f[0],f[1],-v])
            f2.append([f[2],f[3],-(v+1)])
            f2.append([f[4],f[5],-(v+2)])
            f2.append([f[6],v,-(v+3)])
            f2.append([v+1,v+2,v+3])
            v += 4
        elif len(f) == 8:
            f2.append([f[0], f[1], -v])
            f2.append([f[2], f[3], -(v+1)])
            f2.append([f[4], f[5], -(v+2)])
            f2.append([f[6], f[7], -(v+3)])
            f2.append([v, v+1, v+4])
            f2.append([v+2, v+3, -(v+4)])
            v += 5
        elif len(f) == 9:
            f2.append([f[0], f[1], -v])
            f2.append([f[2], f[3], -(v+1)])
            f2.append([f[4], f[5], -(v+2)])
            f2.append([f[6], f[7], -(v+3)])
            f2.append([f[8], v, -(v+4)])
            f2.append([v+1, v+2, (v+5)])
            f2.append([v+3, v+4, -(v+5)])
            v += 6
        elif len(f) == 10:
            f2.append([f[0], f[1], -v])
            f2.append([f[2], f[3], -(v+1)])
            f2.append([f[4], f[5], -(v+2)])
            f2.append([f[6], f[7], -(v+3)])
            f2.append([f[8], f[9], -(v+4)])
            f2.append([v, v+1, -(v+5)])
            f2.append([v+2, v+3, -(v+6)])
            f2.append([v+4, v+5, v+6])
            v += 7
        else:
            raise(NotImplementedError)
    return f2, v


# add unsolved nodes and solving strategy
def sat_validation(sat_array, solution, final_iteration):
    validation = True
    int_list = 0
    bool_list = []
    for clause in sat_array:
        status = False
        for val in clause:
            if val < 0:
                status = status or (solution[-val-1] == 0)
            else:
                status = status or (solution[val-1] == 1)
        validation = validation and status
        bool_list.append(status)
    if final_iteration:
        int_list = sum([int(x == True) for x in bool_list])
    else:
        int_list = len(sat_array)
    return validation,int_list

def sat_validation_v2(sat_array, solution, final_iteration):
    bool_list = []
    validation = True

    for clause in sat_array:
        status = any((solution[-val-1] == 0) if val < 0 else (solution[val-1] == 1) for val in clause)
        bool_list.append(status)
        validation = validation and status
        if not final_iteration:
            if not validation:
                break

    int_list = sum(bool_list) if final_iteration else len(sat_array)

    return validation, int_list


def energy_calculation(sat_array, solution):
    validation = True
    bool_list = []
    for clause in sat_array:
        status = False
        for val in clause:
            if val < 0:
                status = status or (solution[-val-1] == 0)
            else:
                status = status or (solution[val-1] == 1)
        validation = validation and status
        bool_list.append(status)
    return validation

def unitProp(cnf, index, value):
    simplified_cnf = []
    sign = +1 if value else -1
    for c in cnf:
        if sign*index in c:
            pass
        elif -sign*index in c:
            remaining =  list( set(c) - set([-sign*index]))
            if remaining != []:
                simplified_cnf.append(remaining)
        else:
            simplified_cnf.append(c)
    return simplified_cnf

def unitProp2(cnf, index, value):
    simplified_cnf = []
    bool_vars = set()
    anc_count = 0
    sign = +1 if value else -1
    for c in cnf:
        fast_c = set(c)
        if sign*index in fast_c:
            pass
        elif -sign*index in fast_c:
            remaining =  list( fast_c - set([-sign*index])).copy()
            if remaining != []:
                simplified_cnf.append(remaining)
                bool_vars.update(set(map(abs, remaining)))
                anc_count += estimate_ancillaries(remaining)
        else:
            simplified_cnf.append(c.copy())
            bool_vars.update(set(map(abs, c)))
            anc_count += estimate_ancillaries(c) #ancillaries
    return simplified_cnf, (len(bool_vars) + anc_count), len(bool_vars)

def unitProp3(cnf, index, value):
    simplified_cnf = []
    bool_vars = set()
    anc_count = 0
    sign = +1 if value else -1
    for c in cnf:
        fast_c = set(c)
        if sign*index in fast_c:
            pass
        elif -sign*index in fast_c:
            remaining =  list( fast_c - set([-sign*index])).copy()
            if remaining != []:
                simplified_cnf.append(remaining)
                bool_vars.update(set(map(abs, remaining)))
                anc_count += ancillary_counter(remaining)
        else:
            simplified_cnf.append(c.copy())
            bool_vars.update(set(map(abs, c)))
            anc_count += ancillary_counter(c) #ancillaries
    return simplified_cnf, (len(bool_vars) + anc_count)

def bulkUnitProp(cnf, nodes, values):
    simplified_cnf = []
    fast_nodes = set(nodes) # searching in a set is much faster
    bool_vars = set()
    anc_count = 0
    #print(f'input_clause_number = {len(cnf)},input cnf = {cnf}')
    #print(f'Propagated Nodes = {nodes}')
    for c in cnf:
        new_clause = set()
        clause_simplified = False
        #print(f'Simplifying {c}')
        for each_literal in c: #positive
            if each_literal in fast_nodes: #propagating a variable
                if values[each_literal-1]: #one value is 1, clause is gone
                    clause_simplified = True
                    break
                else: # value is 0, dont add the literal
                    pass
            elif -each_literal in fast_nodes: #negative
                if not values[-each_literal-1]: #not value is 0, clause is gone
                    clause_simplified = True
                    break
                else: # not value is 1, dont add the literal
                    pass
            else:
                new_clause.add(each_literal)
        if not clause_simplified:
            #print(f'Simplified to {new_clause}')
            if(len(new_clause) != 0):
                simplified_cnf.append(list(new_clause))
                bool_vars.update(set(map(abs, new_clause)))
                anc_count += estimate_ancillaries(new_clause)
        else:
            #print('Clause removed')
            pass
    #print(f'simplified_clause_number = {len(simplified_cnf)},input cnf = {simplified_cnf}')
    var_count = (len(bool_vars) + anc_count)
    #print(f'Ancillary count = {anc_count}')
    if var_count <= SOLVER_SPINS: #fits to hardware, not wanted after bulk
        return False, cnf
    else: # failed to fit to the hardware
        return True, simplified_cnf

def bulkUnitProp2(cnf, nodes, values):
    simplified_cnf = []
    fast_nodes = set(nodes) # searching in a set is much faster
    bool_vars = set()
    anc_count = 0
    #print(f'input_clause_number = {len(cnf)},input cnf = {cnf}')
    #print(f'Propagated Nodes = {nodes}')
    for c in cnf:
        new_clause = set()
        clause_simplified = False
        #print(f'Simplifying {c}')
        for each_literal in c: #positive
            if each_literal in fast_nodes: #propagating a variable
                if values[each_literal-1]: #one value is 1, clause is gone
                    clause_simplified = True
                    break
                else: # value is 0, dont add the literal
                    pass
            elif -each_literal in fast_nodes: #negative
                if not values[-each_literal-1]: #not value is 0, clause is gone
                    clause_simplified = True
                    break
                else: # not value is 1, dont add the literal
                    pass
            else:
                new_clause.add(each_literal)
        if not clause_simplified:
            #print(f'Simplified to {new_clause}')
            if(len(new_clause) != 0):
                simplified_cnf.append(list(new_clause))
                bool_vars.update(set(map(abs, new_clause)))
                anc_count += ancillary_counter(new_clause) #ancillaries
        else:
            #print('Clause removed')
            pass
    #print(f'simplified_clause_number = {len(simplified_cnf)},input cnf = {simplified_cnf}')
    var_count = (len(bool_vars) + anc_count)
    #print(f'Ancillary count = {anc_count}')
    if var_count <= SOLVER_SPINS: #fits to hardware, not wanted after bulk
        return False, cnf
    else: # failed to fit to the hardware
        return True, simplified_cnf

def initialize_nodes(graph):
    # Get the maximum node label to size the list properly
    max_node_label = max(graph.nodes)

    # Initialize node values as a list with None, indexed from 1 to max_node_label
    node_values = [None] * max_node_label

    # Function to traverse the graph recursively
    def dfs(node, value):
        node_values[node - 1] = value  # Store value at index node-1

        # Randomize the order of neighbors
        neighbors = list(graph.neighbors(node))
        shuffle(neighbors)

        for neighbor in neighbors:
            if node_values[neighbor - 1] is None:  # Only visit unvisited neighbors
                edge_weight = graph[node][neighbor].get('weight', None)
                if edge_weight is not None:  # Only process edges with weights
                    # Assign the same value if weight > 0, opposite if weight < 0
                    neighbor_value = 1-value if edge_weight > 0 else value
                    dfs(neighbor, neighbor_value)

    # First iteration: Pick a random starting node and traverse
    all_nodes = list(graph.nodes)
    shuffle(all_nodes)  # Randomize starting point for traversal

    for node in all_nodes:
        if node_values[node - 1] is None:
            # Assign a random binary value to the first unvisited node
            initial_value = choice([0, 1])
            dfs(node, initial_value)

    # Second iteration: Assign random values to remaining `None` nodes and continue DFS
    for node in all_nodes:
        if node_values[node - 1] is None:
            # Assign a random binary value to unvisited nodes
            initial_value = choice([0, 1])
            dfs(node, initial_value)
    #print(node_values)
    return node_values

def main(input_dir,filename,extension):
    NEIGHBOR_LIMIT = randint(1,50)
    #seed(1)
    cnf, maxvar,ksat = import_cnf(input_dir, filename + extension)
    #spins = [0] * maxvar
    #spins = [randint(0, 1) for _ in range(maxvar)]

    #G = Graph()
    #for n in range(maxvar):
    #    G.add_node(n+1)
    #for c in cnf:
    #    pairs = [(a, b) for idx, a in enumerate(c) for b in c[idx + 1:]]
    #    for pair in pairs:
    #        G.add_edge(abs(pair[0]), abs(pair[1]))
    G = Graph()
#    for n in range(maxvar):
#        G.add_node(n + 1)

    for c in cnf:
        for l in c:
            if not G.has_node(abs(l)):
                G.add_node(abs(l))
        pairs = [(a, b) for idx, a in enumerate(c) for b in c[idx + 1:]]
        for pair in pairs:
            node_a, node_b = abs(pair[0]), abs(pair[1])
            sign = pair[0]*pair[1]>=0
            if True or WEIGHTED_BFS or WEIGHTED_BFSV2 or WEIGHTED_SHUFFLE_BFS or REDUCE_NEIGHBORS or LIMITED_NEIGHBORS:
                if G.has_edge(node_a, node_b):
                    # Increment weight if edge already exists
                    if(sign or not SIGNED_EDGES):
                        G[node_a][node_b]['weight'] += 1
                    else:
                        G[node_a][node_b]['weight'] -= 1
                else:
                    if(sign or not SIGNED_EDGES):
                        G.add_edge(node_a, node_b, weight=1)
                    else:
                        G.add_edge(node_a, node_b, weight=-1)
            else:
                G.add_edge(node_a, node_b)
    original_G = G.copy()
    if SEEK_INITIAL:
        spins = initialize_nodes(original_G)
    else:
        spins = [randint(0, 1) for _ in range(maxvar)]
#    for u, v, d in G.edges(data=True):
#        d['weight'] = abs(d['weight'])  # Absolute values to the weights
    if REDUCE_NEIGHBORS:
        G = reduce_neighbors(G)
    if LIMITED_NEIGHBORS:
        G = limited_neighbors(G,NEIGHBOR_LIMIT)
    if GREEDY_ROOT:
        node_degrees = G.degree()
        sorted_nodes = sorted(node_degrees, key=lambda x: x[1], reverse=True)
        sorted_node_list = [node for node, degree in sorted_nodes]
    all_nodes = list(G.nodes())
    total_varcnt = 0
    hardware_time = 0
    # locking_time = 0
    total_PCIE_time = 0
    total_clamped = 0
    prop_cntr = 0
    local_search = 0
    if(ksat > 2):
        #local_search_limit = (ksat-2)*maxvar*len(cnf)/100
        #local_search_limit = 2*(ksat-2)*maxvar
        local_search_limit = LOCAL_SEARCH_LIMIT
    else:
        local_search_limit = np.inf
        global LOCAL_SEARCH
        LOCAL_SEARCH = False
    previous_root = None
    time_start = time()
    visited_nodes = set()
    unvisited_nodes = set()
    for iteration in (range(1,NUM_ITERS+1)):
        # BFS traversal
        if(len(visited_nodes)<maxvar and GREEDY_ROOT):
            root = None
            while root == None:
                top_node = sorted_node_list.pop(0)
                if top_node in (set(G.nodes)-visited_nodes):
                    root = top_node
        elif(GREEDY_RANDOM_ROOT):
            #print('visited nodes = ',visited_nodes)
            unvisited_nodes = set(G.nodes)-visited_nodes
            #print('unvisited nodes = ',unvisited_nodes)
            if (len(unvisited_nodes)==0):
                unvisited_nodes = set(G.nodes)
            root = sample(unvisited_nodes, 1)[0]  # Select a random element
            #print('Selected root = ', root)
        else:
            #root = randint(1, maxvar)
            root = choice(all_nodes)
        #print(f'original nodes = {nodes} ')
        if NO_SAME_ROOT_TWICE: # dont select the same root twice
            while root == previous_root:
                root = choice(all_nodes)
                #root = randint(1,maxvar)
            previous_root = root
        if NEIGHBOR_SUFFLE:
            levels = {}
            for node, distance in single_source_shortest_path_length(G, root).items():
                if distance not in levels:
                    levels[distance] = []
                levels[distance].append(node)

            # Shuffle nodes within each level
            for level_nodes in levels.values():
                shuffle(level_nodes)
            shuffled_nodes_by_level = []
            for level in sorted(levels.keys()):
                shuffled_nodes_by_level.extend(levels[level])
            nodes = shuffled_nodes_by_level
            #print(f'shuffled nodes = {nodes} ')
        else:
            if WEIGHTED_BFS:
                edges,nodes = weighted_bfs(G, root)
            if WEIGHTED_BFSV2:
                edges,nodes = weighted_bfsV2(G, root)
            elif WEIGHTED_SHUFFLE_BFS:
                edges,nodes = weighted_bfs_with_shuffle(G, root)
            else:
                edges = bfs_edges(G, root)
                nodes = [root] + [v for u, v in edges]
        if(len(nodes) < maxvar):
            unreachable_nodes = set(G.nodes())-set(nodes)
            nodes.extend(unreachable_nodes)
        # Allocate subproblem CNF
        #surrogate_cnf = deepcopy(cnf)
        surrogate_cnf = [clause[:] for clause in cnf]

        prob_estimated = False
        # Inner loop: freeze more variables until subproblem fits in hardware
        while nodes != []:
            if iteration==1: # First iteration, one by one freeze
                surrogate_cnf,varcnt,real_varcnt = unitProp2(surrogate_cnf, nodes[-1], spins[nodes[-1]-1])
                nodes.pop(-1)
                prop_cntr = prop_cntr + 1
                #print('Frozen count: ',prop_cntr)
            elif prob_estimated == True:
                #print(f'Iteration {i}, One by one freezing after bulk freeze, estimate = {prop_cntr}')
                surrogate_cnf,varcnt,real_varcnt = unitProp2(surrogate_cnf, nodes[-1], spins[nodes[-1]-1])
                nodes.pop(-1)
                #surrogate_cnf3, maxvar3 = sat_kN_to_k3(surrogate_cnf, maxvar)
                #varcnt, _ = calculate_varcnt(surrogate_cnf) # Count 3SAT clause cnt and calculate total varcnt
            else:
                prob_estimated = True
                if(prop_cntr<len(nodes)):
                    estimate_success, surrogate_cnf = bulkUnitProp(surrogate_cnf,nodes[-prop_cntr:], spins)
                    if estimate_success:
                        #print(f'Bulk estimation success, {prop_cntr} variables are frozen at once')
                        del nodes[-prop_cntr:]
                        #surrogate_cnf3, maxvar3 = sat_kN_to_k3(surrogate_cnf, maxvar)
                        varcnt, _ = calculate_varcnt2(surrogate_cnf) # Count 3SAT clause cnt and calculate total varcnt
                    else:
                        #print(f'Bulk estimation failed, {prop_cntr} variables wasted')
                        prop_cntr = prop_cntr-1
                        continue
                else:
                    #print(f'Bulk estimation failed, {prop_cntr} is higher than the CNF size {len(nodes)}')
                    prop_cntr = 0
                    continue
            if varcnt <= SOLVER_SPINS:
                total_varcnt += real_varcnt
                if GREEDY_ROOT or GREEDY_RANDOM_ROOT:
                    if(len(visited_nodes) < maxvar):
                        visited_nodes.update(nodes)
                    else:
                        visited_nodes = set()
                if not (ILP_COMB or FLAT_ILP_COMB):
                    surrogate_cnf3, maxvar3 = sat_kN_to_k3(surrogate_cnf, maxvar)
                    varcnt, surrogate_vars = calculate_varcnt2(surrogate_cnf3)
                    assert(varcnt <= SOLVER_SPINS)
                    curr_Q = get_idt(surrogate_cnf3, maxvar3) # Calculate maxvar
                    break
                else:
                    varcnt, surrogate_vars = calculate_varcnt2(surrogate_cnf)
                    curr_Q, ancillary_index = get_idt2(surrogate_cnf, maxvar) # Calculate maxvar
                    slackVarIndex = ancillary_index
                    for clause in surrogate_cnf:
                        if(len(clause) > 3):
                            if ILP_COMB:
                                slackVarIndex = addClause(curr_Q, clause, maxvar, slackVarIndex)
                            elif FLAT_ILP_COMB:
                                slackVarIndex = addClauseV2(curr_Q, clause, maxvar, slackVarIndex)
                    break

        if varcnt > SOLVER_SPINS:
            continue

        # Interpret results
        sanitized_Q = {}
        for (i,j),w in curr_Q.items():
            if w != 0:
                sanitized_Q[(i,j)] = w
        # Solve on chip
        tic = time()
        state_sample_int,PCIE_time,clamped = cobi_sample(sanitized_Q, surrogate_vars, HWDEBUG)
        toc = time()
        hardware_time += toc - tic
        # locking_time += lock_waiting_time
        total_clamped += clamped
        total_PCIE_time += PCIE_time
        # Update full solution
        sat_solution = {}
        try:
            for t in range(maxvar):
                if (t+1) in surrogate_vars:
                    sat_solution[t] = state_sample_int[t]
                    spins[t] = state_sample_int[t]
                else:
                    sat_solution[t] = spins[t]
        except Exception as e:
            print('Error = ',e)

            with open('./results/error_details.log','a') as f:
                f.write(f'Obtained results from the chip = {state_sample_int}\n')
                f.write(f'Surrogate variables = {[t-1 for t in sorted(surrogate_vars)]}\n')
                f.write(f'Maxvar = {maxvar}\n')
                f.write(f'Selected variable is = {t}\n')
                f.write(f'SAT Solution = {sat_solution}\n\n')


        # Copy surrogate spins back to the spins
        sat,valid_list = sat_validation_v2(cnf, sat_solution,iteration==NUM_ITERS)
        if sat:
            break
        elif LOCAL_SEARCH:
            if (iteration % local_search_limit == 0):
                local_search += 1
                #spins = initialize_nodes(G)
                #spins = [randint(0, 1) for _ in range(maxvar)]
                if SEEK_INITIAL:
                    spins = initialize_nodes(original_G)
                else:
                    spins = [randint(0, 1) for _ in range(maxvar)]

    #print('Iterations:', iteration, f'Time: {time()-time_start:4.6f}s')
    #print(f'{sat},{100*((valid_list)/len(cnf))},{iteration},{time()-time_start}')
    return sat,100*((valid_list)/len(cnf)),iteration,time()-time_start,hardware_time

#,local_search,total_PCIE_time,total_clamped/iteration,total_varcnt/iteration, NEIGHBOR_LIMIT

if __name__=="__main__":
    print(main(input_dir,filename,extension))
