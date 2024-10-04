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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define DEVICE_FILE_TEMPLATE "/dev/cobi_pcie_card%d"

#define COBI_WEIGHT_MATRIX_DIM 46
#define COBI_PROGRAM_MATRIX_DIM 52
#define COBI_SHIL_VAL 7

typedef struct CobiOutput {
    bool *raw_bits;
    int problem_id;
    int *spins;
    int energy;
    int core_id;
} CobiOutput;

typedef struct CobiData {
    size_t probSize;
    size_t w;
    size_t h;
    int **programming_bits;
    uint32_t *serialized_program;
    int serialized_len;

    bool *chip1_output;
    int chip1_problem_id;
    int *chip1_spins;
    int chip1_energy;
    int chip1_core_id;

    bool *chip2_output;
    int chip2_problem_id;
    int *chip2_spins;
    int chip2_energy;
    int chip2_core_id;

    uint32_t process_time;

    int sample_test_count;
} CobiData;

typedef struct CobiSubSamplerParams {
    int device_id;
    int num_samples;
    bool use_polling;
    bool descend;
} CobiSubSamplerParams;

/* typedef struct CobiData { */
/*     size_t probSize; */
/*     size_t w; */
/*     size_t h; */
/*     int **programming_bits; */

/*     uint8_t *chip1_output; */
/*     uint8_t *chip2_output; */

/*     int *chip1_spins; */
/*     int *chip2_spins; */

/*     uint32_t process_time; */

/*     int num_samples; */
/*     int64_t chip_delay; */
/*     bool descend; */

/*     int sample_test_count; */
/* }  CobiData;*/

bool cobi_established(const char*);
int cobi_init(int);
void cobi_close(void);
void cobi_solver(int cobi_device_id, double **, int, int8_t *, int, bool);



// For testing: to be removed
    void __cobi_print_write_time();
    void __cobi_print_read_time();
    void __cobi_print_subprob_zero_count();

#ifdef __cplusplus
}
#endif
