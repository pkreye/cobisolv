import numpy as np
from random import randint, seed
from networkx import Graph, bfs_edges
from copy import deepcopy
from collections import defaultdict
from dimod.utilities import qubo_to_ising
from math import ceil,log2
from time import time
from wrapper import w_solveQ, w_submit_problem, w_read_result, w_read_result_blocking

input_dir = "./QUICC_Datasets_3SAT/batch-03"
filename = "3block-like-rand-00000-0027"
extension = '.cnf'

NUM_ITERS = 1000
SOLVER_SPINS = 45
HWDEBUG = 0
LF_SIZE = 1
SCALE = 4
DEVICE = 1
# 0 for "/dev/cobi_pcie_card0"
# 1 for "/dev/cobi_pcie_card1"
# 2 for "/dev/cobi_pcie_card2"
# 3 for "/dev/cobi_pcie_card3"


def cobi_sample(Q_orig, vars_init=[], HWDEBUG=False, DEVICE=0):
    variables = vars_init
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
    for ix, iy in np.ndindex(mtx.shape):
        if (ix,iy) in J.keys():
            if abs((SCALE*J[ix, iy])) > 6:
                J[ix, iy] = np.sign(J[ix, iy]) * 6
            else:
                J[ix, iy] = (SCALE*J[ix, iy])
            mtx[ix][iy] = mtx[iy][ix] = -J[ix, iy]

    ising = {'Q': [[int(0)] * 46] * 46, 'debug': HWDEBUG}
    ising['Q'] = mtx.tolist()

    # result = {'problem_id': 0, 'energy': 0, 'spins': [0] * 46, 'core_id':0}
    # result = w_solveQ(ising,result,DEVICE)

    w_submit_problem(DEVICE, ising)
    result = w_read_result_blocking(DEVICE)

    # print(result)

    best_sample = result["spins"]
    best_sample.reverse()
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

    return samples


def import_cnf(input_dir,filename):
    my_file = open(input_dir + "/" + filename, "r")
    data = my_file.read()
    in_data = data.split("\n")

    cnf = list()
    cnf.append(list())
    maxvar = 0

    for line in in_data:
        tokens = line.split()
        if len(tokens) != 0 and tokens[0] not in ("p", "c", "%", "0"):
            for tok in tokens:
                lit = int(tok)
                maxvar = max(maxvar, abs(lit))
                if lit == 0:
                    cnf.append(list())
                else:
                    cnf[-1].append(lit)
    assert len(cnf[-1]) == 0
    cnf.pop()
    return cnf, maxvar

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

def bulkUnitProp(cnf, nodes, values):
    simplified_cnf = []
    fast_nodes = set(nodes) # searching in a set is much faster
    bool_vars = set()
    anc_count = 0
    #print(f'input_clause_number = {len(cnf)},input cnf = {cnf}')
    #print(f'Propagated Nodes = {nodes}')
    for c in cnf:
        new_clause = list()
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
                new_clause.append(each_literal)
        if not clause_simplified:
            #print(f'Simplifyed to {new_clause}')
            simplified_cnf.append(new_clause)
            for l in new_clause:
                bool_vars.add(abs(l))
            if len(new_clause) == 3: # chancellor
                anc_count += 1
            elif len(new_clause) == 4: # chancellor
                anc_count += 3
            assert(len(new_clause)<5)
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

def calculate_varcnt(cnf):
    bool_vars = []
    anc_count = 0
    for c in cnf:
        for l in c:
            if abs(l) not in bool_vars:
                bool_vars.append(abs(l))
        assert(len(c) <= 3)
        if len(c) == 3:
            anc_count += 1
    return (len(bool_vars) + anc_count), bool_vars

def main(input_dir,filename,extension,DEVICE):
    seed(1)
    cnf, maxvar = import_cnf(input_dir, filename + extension)

    spins = [0] * maxvar

    G = Graph()
    for n in range(maxvar):
        G.add_node(n+1)
    for c in cnf:
        pairs = [(a, b) for idx, a in enumerate(c) for b in c[idx + 1:]]
        for pair in pairs:
            G.add_edge(abs(pair[0]), abs(pair[1]))

    time_start = time()
    hardware_time = 0
    prop_cntr = 0
    for iteration in (range(1,NUM_ITERS+1)):
        # BFS traversal
        root = randint(1, maxvar)

        edges = bfs_edges(G, root)
        nodes = [root] + [v for u, v in edges]
        # Allocate subproblem CNF
        surrogate_cnf = deepcopy(cnf)

        prob_estimated = False
        # Inner loop: freeze more variables until subproblem fits in hardware
        while nodes != []:
            if iteration==1: # First iteration, one by one freeze
                surrogate_cnf = unitProp(surrogate_cnf, nodes[-1], spins[nodes[-1]-1])
                nodes.pop(-1)
                prop_cntr = prop_cntr + 1
                surrogate_cnf3, maxvar3 = sat_kN_to_k3(surrogate_cnf, maxvar)
                varcnt, surrogate_vars = calculate_varcnt(surrogate_cnf3) # Count 3SAT clause cnt and calculate total varcnt
            elif prob_estimated == True:
                #print(f'Iteration {i}, One by one freezing after bulk freeze, estimate = {prop_cntr}')
                surrogate_cnf = unitProp(surrogate_cnf, nodes[-1], spins[nodes[-1]-1])
                nodes.pop(-1)
                surrogate_cnf3, maxvar3 = sat_kN_to_k3(surrogate_cnf, maxvar)
                varcnt, surrogate_vars = calculate_varcnt(surrogate_cnf3) # Count 3SAT clause cnt and calculate total varcnt
            else:
                prob_estimated = True
                if(prop_cntr<len(nodes)):
                    estimate_success, surrogate_cnf = bulkUnitProp(surrogate_cnf,nodes[-prop_cntr:], spins)
                    if estimate_success:
                        #print(f'Bulk estimation success, {prop_cntr} variables are frozen at once')
                        del nodes[-prop_cntr:]
                        surrogate_cnf3, maxvar3 = sat_kN_to_k3(surrogate_cnf, maxvar)
                        varcnt, surrogate_vars = calculate_varcnt(surrogate_cnf3) # Count 3SAT clause cnt and calculate total varcnt
                    else:
                        #print(f'Bulk estimation failed, {prop_cntr} variables wasted')
                        prop_cntr = prop_cntr-1
                        continue
                else:
                    #print(f'Bulk estimation failed, {prop_cntr} is higher than the CNF size {len(nodes)}')
                    prop_cntr = 0
                    continue
            if varcnt <= SOLVER_SPINS:
                curr_Q = get_idt(surrogate_cnf3, maxvar3) # Calculate maxvar
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
        surrogate_vars_zero_indexed = [x-1 for x in surrogate_vars]
        state_sample_int = cobi_sample(sanitized_Q, surrogate_vars_zero_indexed, HWDEBUG, DEVICE)
        toc = time()
        hardware_time = hardware_time + toc - tic
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

            with open('error_details.log','a') as f:
                f.write(f'Obtained results from the chip = {state_sample_int}\n')
                f.write(f'Surrogate variables = {[t-1 for t in sorted(surrogate_vars)]}\n')
                f.write(f'Maxvar = {maxvar}\n')
                f.write(f'Selected variable is = {t}\n')
                f.write(f'SAT Solution = {sat_solution}\n\n')


        # Copy surrogate spins back to the spins
        sat,valid_list = sat_validation(cnf, sat_solution,iteration==NUM_ITERS)
        if sat:
            break

    #print('Iterations:', iteration, f'Time: {time()-time_start:4.6f}s')
    #print(f'{sat},{100*((valid_list)/len(cnf))},{iteration},{time()-time_start}')
    return sat,100*((valid_list)/len(cnf)),iteration,time()-time_start,hardware_time

def __test():
    res = main(input_dir,filename,extension,DEVICE)
    return res

if __name__=="__main__":
    __test()
