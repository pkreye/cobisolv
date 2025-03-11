/*
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

#include "extern.h"  // qubo header file: global variable declarations
#include "macros.h"
#include "pci.h"
#include "util.h"
#include "cobi.h"

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>


#include <fcntl.h>
#include <glob.h>
#include <unistd.h>

#include <time.h>
#include <omp.h>

#include <cobiserv_client.h>


// Local declarations
// int cobi_read(int cobi_id, CobiOutput *output, bool wait_for_result);


// TESTING: To be removed

static double write_cum_time = 0;
static double read_cum_time = 0;
static int subprob_zero_count = 0;

void __cobi_print_write_time()
{
    printf("cumulative program write time: %f\n", write_cum_time);
}
void __cobi_print_read_time()
{
    printf("cumulative program read time: %f\n", read_cum_time);
}
void __cobi_print_subprob_zero_count()
{
    printf("Subprob zero count: %d\n", subprob_zero_count);
}

// END TESTING


// Misc utility function
void _print_array1d_uint8(uint8_t *a, int len)
{
    for (int i = 0; i < len; i++) {
        printf("%X ", a[i]);
    }
    printf("\n");
}

void print_problem_matrix(int a[][COBI_HW_NUM_SPINS])
{
    int row, col;
    for (row = 0; row < COBI_HW_NUM_SPINS; row++) {
        printf("%02d: ", row);
        for (col = 0; col < COBI_HW_NUM_SPINS; col++) {
            printf("%d ", a[row][col]);
        }
        printf("\n");
    }
}

int *_malloc_array1d(int len)
{
    int* a = (int*)malloc(sizeof(int *) * len);
    if (a == NULL) {
        fprintf(stderr, "Bad malloc %s %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
        exit(1);
    }

    int i;
    for (i = 0; i < len; i++) {
        a[i] = 0;
    }

    return a;
}

// TODO switch to using util.c's malloc2D in place of specifically typed mallocs

void zero_array2d(int **a, int w, int h)
{
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            a[i][j] = 0;
        }
    }
}

// TODO fix all the type based functions... quick hack is getting out of hand
void zero_array2d_uint8(uint8_t **a, int w, int h)
{
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            a[i][j] = 0;
        }
    }
}

double **_malloc_array2d_double(int w, int h)
{
    double** a = (double**)malloc(sizeof(double *) * h);
    if (a == NULL) {
        fprintf(stderr, "Bad malloc %s %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
        exit(1);
    }

    int i, j;
    for (i = 0; i < h; i++) {
        a[i] = (double*)malloc(sizeof(double) * w);
        for (j = 0; j < w; j++) {
            a[i][j] = 0;
        }
    }

    return a;
}

int **_malloc_array2d_int(int w, int h)
{
    int** a = (int**)malloc(sizeof(int *) * h);
    if (a == NULL) {
        fprintf(stderr, "Bad malloc %s %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
        exit(1);
    }

    int i, j;
    for (i = 0; i < h; i++) {
        a[i] = (int*)malloc(sizeof(int) * w);
        for (j = 0; j < w; j++) {
            a[i][j] = 0;
        }
    }

    return a;
}

uint8_t **_malloc_array2d_uint8(int w, int h)
{
    uint8_t** a = (uint8_t**)malloc(sizeof(uint8_t *) * h);
    if (a == NULL) {
        fprintf(stderr, "Bad malloc %s %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
        exit(1);
    }

    int i, j;
    for (i = 0; i < h; i++) {
        a[i] = (uint8_t*)malloc(sizeof(uint8_t) * w);
        for (j = 0; j < w; j++) {
            a[i][j] = 0;
        }
    }

    return a;
}


void _free_array2d(void **a, int h) {
    int i;
    for (i = 0; i < h; i++) {
        free(a[i]);
    }
    free(a);
}

// Cobi data structure functions

void cobi_output_clear(CobiOutput *o, size_t num_spins)
{
    for (size_t i = 0; i < num_spins; i++) {
        o->spins[i] = 0;
    }

    o->problem_id = -1;
    o->core_id    = -1;
    o->energy     = 0;
}

CobiOutput *cobi_output_mk_default()
{
    CobiOutput *output = (CobiOutput*) malloc(sizeof(CobiOutput));
    memset(output->spins, 0, COBI_NUM_SPINS * sizeof(int));
    output->problem_id = -1;
    output->core_id    = -1;
    output->energy     = 0;
    return output;
}

/* CobiData *cobi_data_mk(size_t num_spins, size_t num_samples) */
/* { */
/*     CobiData *d = (CobiData*)malloc(sizeof(CobiData)); */
/*     d->probSize = num_spins; */
/*     d->w = COBI_PROGRAM_MATRIX_WIDTH; */
/*     d->h = COBI_PROGRAM_MATRIX_HEIGHT; */
/*     // prepare enough outputs to hold all samples */
/*     d->num_outputs = num_samples; */
/*     d->chip_output = (CobiOutput**) malloc(sizeof(CobiOutput*) * num_samples); */
/*     for (size_t i = 0; i < num_samples; i++) { */
/*         d->chip_output[i] = cobi_output_mk_default(); */
/*     } */
/*     return d; */
/* } */

void free_cobi_output(CobiOutput *o)
{
    // free(o->spins);
    free(o);
}

/* void free_cobi_data(CobiData *d) */
/* { */
/*     // _free_array2d((void**)d->programming_bits, d->h); */
/*     // free(d->serialized_program); */
/*     for(size_t i = 0; i < d->num_outputs; i++) { */
/*         free_cobi_output(d->chip_output[i]); */
/*     } */
/*     free(d); */
/* } */

// Produces normalized problem, moves linear terms from diagonal to first row and col
void cobi_norm_scaled(
    int norm[][COBI_HW_NUM_SPINS],
    double ising[][COBI_NUM_SPINS],
    double scale
) {
    const int WEIGHT_MAX = 14;
    const int WEIGHT_MIN = -14;
    for (size_t i = 0; i < COBI_NUM_SPINS; i++) {
        for (size_t j = i; j < COBI_NUM_SPINS; j++) {
            double cur_val = ising[i][j];

            // indicies into norm matrix
            int row = i + 1;
            int col = j + 1;

            int scaled_val = round(scale * cur_val);
            if (cur_val > 0) {
                scaled_val = MIN(scaled_val, WEIGHT_MAX);
            } else {
                scaled_val = MAX(scaled_val, WEIGHT_MIN);
            }

            if (cur_val == 0) {
                norm[row][col] = 0;
                norm[col][row] = 0;
            } else if (i == j) {
                // Split linear term between first row and first column
                int symmetric_val = scaled_val / 2;
                norm[0][col] = symmetric_val;
                norm[row][0] = symmetric_val + (scaled_val % 2);
            } else {
                int symmetric_val = scaled_val / 2;
                norm[row][col] = symmetric_val;
                norm[col][row] = symmetric_val + (scaled_val % 2);
            }
        }
    }
}

// Produces normalized problem, moves linear terms from diagonal to first row and col
void cobi_norm_nonlinear(
    int norm[][COBI_HW_NUM_SPINS],
    double ising[][COBI_NUM_SPINS],
    double scale
) {
    const int WEIGHT_MAX = 14;
    const int WEIGHT_MIN = -14;
    for (size_t i = 0; i < COBI_NUM_SPINS; i++) {
        for (size_t j = i; j < COBI_NUM_SPINS; j++) {
            double cur_val = ising[i][j];

            // indicies into norm matrix
            int row = i + 1;
            int col = j + 1;

            if (cur_val == 0) {
                norm[row][col] = 0;
                norm[col][row] = 0;
            } else {
                // Non-zero case
                cur_val = scale * cur_val;
                double fsig = (cur_val / (1.0 + abs(cur_val)));
                int scaled_val = round(WEIGHT_MAX * fsig);
                if (cur_val > 0) {
                    scaled_val = MIN(scaled_val, WEIGHT_MAX);
                } else {
                    scaled_val = MAX(scaled_val, WEIGHT_MIN);
                }

                if (i == j) {
                    // Split linear term between first row and first column
                    int symmetric_val = scaled_val / 2;
                    norm[0][col] = symmetric_val;
                    norm[row][0] = symmetric_val + (scaled_val % 2);
                } else {
                    int symmetric_val = scaled_val / 2;
                    norm[row][col] = symmetric_val;
                    norm[col][row] = symmetric_val + (scaled_val % 2);
                }
            }
        }
    }
}

// Produces normalized problem, moves linear terms from diagonal to first row and col
void cobi_norm_linear(
    int norm[][COBI_HW_NUM_SPINS],
    double ising[][COBI_NUM_SPINS],
    double scale
) {
    double min = 0;
    double max = 0;
    double cur_v = 0;

    for (size_t i = 0; i < COBI_NUM_SPINS; i++) {
        for (size_t j = i; j < COBI_NUM_SPINS; j++) {
            cur_v = ising[i][j];

            if (cur_v > max) max = cur_v;
            if (cur_v < min) min = cur_v;
        }
    }

    // Linear scaling to range [-14, 14]
    // (y + 14) / (x - min) = 28 / (max - min)
    // y = (28 / (max - min)) * (x - min) - 14

    /* const double upscale_factor = 1.5; */
    const int top = 14;
    const int bot = -14;
    const int range = top - bot;

    for (size_t i = 0; i < COBI_NUM_SPINS; i++) {
        for (size_t j = i; j < COBI_NUM_SPINS; j++) {
            cur_v = ising[i][j];

            // indicies into norm matrix
            int row = i + 1;
            int col = j + 1;

            // linearly interpolate and upscale
            int scaled_val = round(
                scale * ((range * (cur_v - min) / (max - min)) - top)
            );

            // clamp
            if (scaled_val < 0 && scaled_val < bot) {
                scaled_val = bot;
            } else if (scaled_val > 0 && scaled_val > top) {
                scaled_val = top;
            }

            if (cur_v == 0) {
                norm[row][col] = 0;
                norm[col][row] = 0;
            } else if (i == j) {
                // Split linear term between first row and first column
                int symmetric_val = scaled_val / 2;
                norm[0][col] = symmetric_val;
                norm[row][0] = symmetric_val + (scaled_val % 2);
            } else {
                int symmetric_val = scaled_val / 2;
                norm[row][col] = symmetric_val;
                norm[col][row] = symmetric_val + (scaled_val % 2);
            }
        }
    }
}

void cobi_norm_val(
    int norm_ising[][COBI_HW_NUM_SPINS],
    double ising[][COBI_NUM_SPINS],
    CobiEvalStrat strat,
    int prob_num
) {
    switch (strat) {
    case COBI_EVAL_NORM_LINEAR: {
        const double scales[5] = {0.5, 1, 1.5, 2, 4};
        cobi_norm_linear(norm_ising, ising, scales[prob_num]);
        break;
    }
    case COBI_EVAL_NORM_SCALED: {
        const double scales[5] = {0.5, 1, 1.5, 2, 4};
        cobi_norm_scaled(norm_ising, ising, scales[prob_num]);
        break;
    }
    case COBI_EVAL_NORM_NONLINEAR: {
        const double scales[5] = {2, 1.5, 1, 0.5, 0.1};
        cobi_norm_nonlinear(norm_ising, ising, scales[prob_num]);
        break;
    }
    case COBI_EVAL_NORM_MIXED: {
        switch (prob_num) {
        case 0:
            cobi_norm_linear(norm_ising, ising, 2);
            break;
        case 1:
            cobi_norm_linear(norm_ising, ising, 4);
            break;
        case 2:
            cobi_norm_scaled(norm_ising, ising, 1);
            break;
        case 3:
            cobi_norm_nonlinear(norm_ising, ising, 2);
            break;
        case 4:
            cobi_norm_nonlinear(norm_ising, ising, 0.5);
            break;
        default:
            perror("Bad prob num\n");
            exit(1);
            break;
        }
        break;
    }
    default:
        // default to linear with no scaling
        cobi_norm_linear(norm_ising, ising, 1);
        break;
    }
}

void ising_solution_from_qubo_solution(int8_t ising_soln[COBI_NUM_SPINS], int8_t *qubo_soln, int len)
{
    // Convert solution to ising formulation
    for(int i = 0; i < len; i++) {
        if (qubo_soln[i] == 0) {
            ising_soln[i] = 1;
        } else {
            ising_soln[i] = -1;
        }
    }
}

void qubo_solution_from_ising_solution(int8_t *qubo_soln, int8_t *ising_soln, int len)
{
    for(int i = 0; i < len; i++) {
        if (ising_soln[i] == 1) {
            qubo_soln[i] = 1;
        } else {
            qubo_soln[i] = 0;
        }
    }
}


// Converts problem from qubo to ising formulation.
// Result is stored in `ising`.
void ising_from_qubo(double ising[][COBI_NUM_SPINS], double **qubo)
{
    for (int i = 0; i < COBI_NUM_SPINS; i++) {
        ising[i][i] = qubo[i][i] / 2;

        for (int j = i + 1; j < COBI_NUM_SPINS; j++) {
            // convert quadratic terms
            ising[i][j] = qubo[i][j] / 4;
            ising[j][i] = qubo[j][i] / 4;

            // add rest of linear term
            ising[i][i] += ising[i][j] + ising[j][i];
        }
    }
}

void cobi_solver(
    CobiSubSamplerParams *params, double **qubo, int num_spins, int8_t *qubo_solution
) {
    if (num_spins != COBI_NUM_SPINS) {
        printf(
            "Quitting.. cobi_solver called with size %d. Must be %d.\n",
            num_spins, COBI_NUM_SPINS
        );
        exit(2);
    }

    double reftime = 0;
    int8_t ising_solution[COBI_NUM_SPINS] = {0};
    double ising[COBI_NUM_SPINS][COBI_NUM_SPINS] = {0};
    CobiProblemNative norm_ising = {{0}};

    // int num_probs; // to be run in parallel
    // if (params->eval_strat == COBI_EVAL_SINGLE) {
    //     num_probs = 1;
    // } else {
    //     num_probs = 5;
    // }


    // Convert problem from QUBO to Ising
    ising_from_qubo(ising, qubo);

    // Convert solution from QUBO to Ising
    ising_solution_from_qubo_solution(ising_solution, qubo_solution, num_spins);

    // Scale problem
    cobi_norm_val(norm_ising.problem, ising, COBI_EVAL_SINGLE, 0);

    if (Verbose_ > 3) {
        printf("--\n");
        // _print_array1d_uint8(cntrl_nibbles, COBI_CONTROL_NIBBLES_LEN);

        printf("Normalized ising:\n");
        print_problem_matrix(norm_ising.problem);
        printf("--\n");
    }


    reftime = omp_get_wtime();
    CobiResult result;
    int rc = _run_native_problem(&norm_ising, &result);
    if (rc != 0) {
        printf("ERROR running problem: %d\n", rc);
        exit(rc);
    }

    write_cum_time += omp_get_wtime() - reftime;

    // Copy best result into solution
    for (size_t i = 0; i < COBI_PROB_NUM_SPINS; i++) {
        // map binary spins to {-1,1} values
        /* ising_solution[i] = result.spins[i] == 0 ? 1 : -1; */
        ising_solution[i] = result.spins[i];
    }

    // Convert ising solution back to QUBO form
    qubo_solution_from_ising_solution(qubo_solution, ising_solution, num_spins);
}

CobiEvalStrat cobi_parse_eval_strat(char *str)
{
    // default value
    CobiEvalStrat strat = COBI_EVAL_SINGLE;

    if (!strncmp(str, COBI_EVAL_STRING_DECOMP_INDEP, COBI_EVAL_STRING_LEN)) {
        strat = COBI_EVAL_NAIVE;
    } else if (!strncmp(str, COBI_EVAL_STRING_DECOMP_INDEP, COBI_EVAL_STRING_LEN)) {
        strat = COBI_EVAL_DECOMP_INDEP;
    } else if (!strncmp(str, COBI_EVAL_STRING_DECOMP_DEP, COBI_EVAL_STRING_LEN)) {
        strat = COBI_EVAL_DECOMP_DEP;
    } else if (!strncmp(str, COBI_EVAL_STRING_NORM_LINEAR, COBI_EVAL_STRING_LEN)) {
        strat = COBI_EVAL_NORM_LINEAR;
    } else if (!strncmp(str, COBI_EVAL_STRING_NORM_SCALED, COBI_EVAL_STRING_LEN)) {
        strat = COBI_EVAL_NORM_SCALED;
    } else if (!strncmp(str, COBI_EVAL_STRING_NORM_NONLINEAR, COBI_EVAL_STRING_LEN)) {
        strat = COBI_EVAL_NORM_NONLINEAR;
    } else if (!strncmp(str, COBI_EVAL_STRING_NORM_MIXED, COBI_EVAL_STRING_LEN)) {
        strat = COBI_EVAL_NORM_MIXED;
    } else if (!strncmp(str, COBI_EVAL_STRING_SINGLE, COBI_EVAL_STRING_LEN)) {
        strat = COBI_EVAL_SINGLE;
    }
    return strat;
}
