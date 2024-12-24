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
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define COBI_DEVICE_NAME_LEN 23 // assumes 2 digit card number and null byte
#define COBI_DEVICE_NAME_TEMPLATE "/dev/cobi_pcie_card%hhu"

#define COBI_WEIGHT_MATRIX_DIM 45
#define COBI_NUM_SPINS 46             // size of weight matrix including the local field
#define COBI_PROGRAM_MATRIX_HEIGHT 51
#define COBI_PROGRAM_MATRIX_WIDTH 52
#define COBI_SHIL_VAL 0
#define COBI_SHIL_INDEX 22 // index of first SHIL row/col in 2d program array

#define COBI_CHIP_OUTPUT_READ_COUNT 3 // number of reads needed to get a full output
#define COBI_CHIP_OUTPUT_LEN 69  // number of bits in a full output

#define COBI_CONTROL_NIBBLES_LEN 52

// FPGA
#define COBIFIVE_FW_ID      0xAA558822
#define COBIFIVE_QUAD_FW_ID 0xAA558823

#define COBI_FPGA_ADDR_FW_ID  1
#define COBI_FPGA_ADDR_FW_REV 2
#define COBI_FPGA_ADDR_READ 4
#define COBI_FPGA_ADDR_CONTROL 8
#define COBI_FPGA_ADDR_WRITE 9
#define COBI_FPGA_ADDR_STATUS 10

#define COBI_FPGA_STATUS_MASK_STATUS 1          // 1 == Controller is busy
#define COBI_FPGA_STATUS_MASK_READ_FIFO_EMPTY 2 // 1 == Read FIFO Empty
#define COBI_FPGA_STATUS_MASK_READ_FIFO_FULL  4 // 1 == Read FIFO Full
#define COBI_FPGA_STATUS_MASK_S_READY 8         // 1 == ready to receive data
#define COBI_FPGA_STATUS_MASK_READ_COUNT 0x7F0  // Read FIFO Count

#define COBI_FPGA_STATUS_VALUE_S_READY 8

#define COBI_FPGA_CONTROL_RESET 0

#define COBI_EVAL_STRING_LEN 2 // min len needed to differentiate the eval options
#define COBI_EVAL_STRING_NAIVE          "naive"
#define COBI_EVAL_STRING_SINGLE         "single"
#define COBI_EVAL_STRING_DECOMP_INDEP   "indep"
#define COBI_EVAL_STRING_DECOMP_DEP     "dep"
#define COBI_EVAL_STRING_NORM_LINEAR    "linear"
#define COBI_EVAL_STRING_NORM_SCALED    "scaled"
#define COBI_EVAL_STRING_NORM_NONLINEAR "nonlinear"
#define COBI_EVAL_STRING_NORM_MIXED     "mixed"

typedef enum {
    COBI_EVAL_NAIVE = 0,
    COBI_EVAL_SINGLE,
    COBI_EVAL_DECOMP_INDEP,
    COBI_EVAL_DECOMP_DEP,
    COBI_EVAL_NORM_LINEAR,
    COBI_EVAL_NORM_SCALED,
    COBI_EVAL_NORM_NONLINEAR,
    COBI_EVAL_NORM_MIXED
} CobiEvalStrat;

typedef enum {
    COBI_RESOLUTION_GREEDY,
    COBI_RESOLUTION_MAJORITY
} CobiResolutionStrat;

typedef struct CobiOutput {
    int problem_id;
    int *spins;
    int energy;
    int core_id;
} CobiOutput;

typedef struct CobiData {
    size_t probSize;
    size_t w;
    size_t h;
    uint8_t **programming_bits;
    /* uint64_t *serialized_program; */
    int serialized_len;

    size_t num_outputs;
    CobiOutput **chip_output;

    uint32_t process_time;

    int sample_test_count;
} CobiData;

typedef struct CobiSubSamplerParams {
    int device_id;
    int num_samples;
    bool descend;
    CobiEvalStrat eval_strat;
    uint8_t shil_val;
    uint16_t cntrl_pid;
    uint16_t cntrl_dco;
    uint16_t cntrl_sample_delay;
    uint16_t cntrl_max_fails;
    uint16_t cntrl_rosc_time;
    uint16_t cntrl_shil_time;
    uint16_t cntrl_weight_time;
    uint16_t cntrl_sample_time;
} CobiSubSamplerParams;

bool cobi_established(const char*);
int cobi_init(int*, int);
void cobi_close(void);
void cobi_solver(CobiSubSamplerParams *, double **, int, int8_t *);

CobiEvalStrat cobi_parse_eval_strat(char *);
// For testing: to be removed
void __cobi_print_write_time();
void __cobi_print_read_time();
void __cobi_print_subprob_zero_count();

#ifdef __cplusplus
}
#endif
