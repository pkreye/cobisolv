/*
 Copyright 2017 D-Wave Systems Inc
 Copyright 2024 Peter Kreye

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/
#pragma once

#include "stdheaders_shim.h"
#include <stdbool.h>
#include "cobi.h"

#ifdef __cplusplus
extern "C" {
#endif

// The pointer type for sub-solver.
// Its arguments are:
// - a 2d double array that is the sub-problem,
// - the size of the sub problem
// - a state vector: on input is the current best state, and should be set to the output state
typedef void (*SubSolver)(double**, int, int8_t*, void*);

// A parameter structure used to pass in optional arguments to the qbsolv: solve method.
typedef struct parameters_t {
    int32_t num_output_solutions;
    // The number of iterations without improvement before giving up
    int32_t repeats;
    // Callback function to solve the sub-qubo
    SubSolver sub_sampler;
    // The maximum size of problem that sub_sampler is willing to accept
    int32_t sub_size;
    // Extra parameter data passed to sub_sampler for callback specific data.
    void* sub_sampler_data;

    char problemName[25];
    int32_t preSearchPassFactor;
    int32_t globalSearchPassFactor;

    int num_sub_prob_threads;

    bool use_cobi;
    int cobi_delay;
    int cobi_num_samples;
    bool cobi_descend;
    bool cobi_parallel_repeat;

    CobiEvalStrat cobi_eval_strat;

    int64_t seed;

    // debug parameters
    int cobi_card_num;

    bool use_polling;

    uint16_t pid;
    uint16_t dco;
    uint16_t sample_delay;
    uint16_t max_fails;
    uint16_t rosc_time;
    uint16_t shil_time;
    uint16_t weight_time;
    uint16_t sample_time;

    uint8_t shil_val;

} parameters_t;

// Get the default values for the optional parameters structure
parameters_t default_parameters(void);

// Callback for `solve` to use tabu on subproblems
void tabu_sub_sample(double** sub_qubo, int subMatrix, int8_t* sub_solution, void*);

// Callback for `solve` to use cobi chip on subproblems
void cobi_sub_sample(double** sub_qubo, int subMatrix, int8_t* sub_solution, void*);

// Callback for `solve` to generate random solutions on subproblems
void rand_sub_sample(double **sub_qubo, int subMatrix, int8_t *sub_solution, void *);

// Callback for `solve` to do return the subsolution passed to the subproblem
void null_sub_sample(double **sub_qubo, int subMatrix, int8_t *sub_solution, void *);

// Entry into the overall solver from the main program
void solve(double** qubo, const int qubo_size, int8_t** solution_list, double* energy_list, int* solution_counts, int* Qindex, int QLEN, parameters_t* param, bool delimitedOutput);

#ifdef __cplusplus
}
#endif
