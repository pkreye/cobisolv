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
#include <unistd.h>

#include <time.h>
#include <omp.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#define COBI_SHIL_ROW 26
#define COBI_CHIP_OUTPUT_LEN 78
#define COBI_CONTROL_NIBBLES_LEN 52

static int cobi_fd = -1;

uint8_t default_control_nibbles[COBI_CONTROL_NIBBLES_LEN] = {
    0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA,
    0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA,
    0, 0, 0xF, 0xF,     // pid
    0, 0, 0, 0x5,     // dco_data
    0, 0, 0xF, 0xF, // sample_delay
    0, 0, 0x1, 0xF,   //  max_fails
    0, 0, 0, 0x3,     // rosc_time
    0, 0, 0, 0xF,   // shil_time
    0, 0, 0, 0x3,     // weight_time
    0, 0, 0xF, 0xD  // sample_time
};

void uint16_to_nibbles(uint16_t val, uint8_t *nibs)
{
    nibs[0] = (val & 0xF000) >> 12;
    nibs[1] = (val & 0x0F00) >> 8;
    nibs[2] = (val & 0x00F0) >> 4;
    nibs[3] = (val & 0x000F);
}

uint8_t *mk_control_nibbles(
    uint16_t pid,
    uint16_t dco,
    uint16_t sample_delay,
    uint16_t max_fails,
    uint16_t rosc_time,
    uint16_t shil_time,
    uint16_t weight_time,
    uint16_t sample_time
) {
    uint8_t *data = (uint8_t *)malloc(sizeof(uint8_t) * COBI_CONTROL_NIBBLES_LEN);
    int index = 0;
    for (index = 0; index < 20; index++) {
        data[index] = 0xA;
    }

    uint16_to_nibbles(pid, &data[index]);
    index += 4;

    uint16_to_nibbles(dco, &data[index]);
    index += 4;

    uint16_to_nibbles(sample_delay, &data[index]);
    index += 4;

    uint16_to_nibbles(max_fails, &data[index]);
    index += 4;

    uint16_to_nibbles(rosc_time, &data[index]);
    index += 4;

    uint16_to_nibbles(shil_time, &data[index]);
    index += 4;

    uint16_to_nibbles(weight_time, &data[index]);
    index += 4;

    uint16_to_nibbles(sample_time, &data[index]);
    index += 4;

    return data;
}

// Misc utility functions

void _print_array2d_int(int **a, int w, int h)
{
    int x, y;
    for (x = 0; x < w; x++) {
        printf("%02d: ", x);
        for (y = 0; y < h; y++) {
            printf("%d ", a[x][y]);
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

double **_malloc_double_array2d(int w, int h)
{
    double** a = (double**)malloc(sizeof(double *) * w);
    if (a == NULL) {
        fprintf(stderr, "Bad malloc %s %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
        exit(1);
    }

    int i, j;
    for (i = 0; i < w; i++) {
        a[i] = (double*)malloc(sizeof(double) * h);
        for (j = 0; j < h; j++) {
            a[i][j] = 0;
        }
    }

    return a;
}

int **_malloc_array2d(int w, int h)
{
    int** a = (int**)malloc(sizeof(int *) * w);
    if (a == NULL) {
        fprintf(stderr, "Bad malloc %s %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
        exit(1);
    }

    int i, j;
    for (i = 0; i < w; i++) {
        a[i] = (int*)malloc(sizeof(int) * h);
        for (j = 0; j < h; j++) {
            a[i][j] = 0;
        }
    }

    return a;
}

void _free_array2d(void **a, int w) {
    int i;
    for (i = 0; i < w; i++) {
        free(a[i]);
    }
    free(a);
}

// FPGA Control interface
void cobi_reset()
{
    pci_write_data write_data;
    // TODO how to reset via pci driver?
    write_data.offset = 8 * sizeof(uint32_t);
    write_data.value = 0x00000000;    // initialize axi interface control
    if (Verbose_ > 2) {
        printf("Initialize cobi chips interface controller\n");
    }

    if (write(cobi_fd, &write_data, sizeof(write_data)) != sizeof(write_data)) {
        perror("Failed to write to device");
        exit(3);
    }
}

// void cobi_enable() {
//     pci_write_data write_data;
//     // fsm control for cobi axi interface
//     write_data.offset = 8 * sizeof(uint32_t);
//     write_data.value = 0x0000000F; // Read enable
//     if (write(cobi_fd, &write_data, sizeof(write_data)) != sizeof(write_data)) {
//         perror("Failed to write fsm control device");
//         exit(-10);
//     }
// }


CobiData *cobi_data_mk(size_t num_spins, bool descend)
{
    CobiData *d = (CobiData*)malloc(sizeof(CobiData));
    d->probSize = num_spins;
    d->w = 52;
    d->h = 52;
    d->programming_bits = _malloc_array2d(d->w, d->h);
    d->chip1_output = (bool*)malloc(sizeof(bool) * COBI_CHIP_OUTPUT_LEN);
    d->chip1_spins = _malloc_array1d(num_spins);

    d->chip1_problem_id = 0;
    d->chip1_energy = 0;
    d->chip1_core_id = 0;

    d->chip2_output = (bool*)malloc(sizeof(bool) * COBI_CHIP_OUTPUT_LEN);
    d->chip2_spins = _malloc_array1d(num_spins);

    d->chip2_problem_id = 0;
    d->chip2_energy = 0;
    d->chip2_core_id = 0;

    d->num_samples = 1;
    d->descend = descend;

    return d;
}

void free_cobi_data(CobiData *d)
{
    _free_array2d((void**)d->programming_bits, d->w);
    free(d->chip1_output);
    free(d->chip1_spins);
    free(d->chip2_output);
    free(d->chip2_spins);
    free(d);
}

uint8_t hex_mapping(int val)
{
    switch (val) {
    case -7:
        return 0xE;
    case -6:
        return 0xC;
    case -5:
        return 0xA;
    case -4:
        return 0x8;
    case -3:
        return 0x6;
    case -2:
        return 0x4;
    case -1:
        return 0x2;
    case 0:
        return 0x0;
    case 1:
        return 0x3;
    case 2:
        return 0x5;
    case 3:
        return 0x7;
    case 4:
        return 0x9;
    case 5:
        return 0xB;
    case 6:
        return 0xD;
    case 7:
        return 0xF;
    default:
        printf("ERROR attempting to map bad weight value: %d\n", val);
        exit(2);
    }
}

uint32_t *cobi_serialize_programming_bits(int **prog_bits, int prog_dim, int *num_words)
{
    int num_bits = prog_dim * prog_dim * 4;
    *num_words = num_bits / 32;
    uint32_t *out_words = (uint32_t*)malloc(*num_words * sizeof(uint32_t));

    int word_index = *num_words - 1;
    int nibble_count = 0;
    out_words[word_index] = 0;

    /* int num_padding_bits = (32 - (num_bits % 32)) % 32; */
    /* for (int i = 0; i < num_padding_bits; i++) { */
    /*     nibble_count++; */
    /* } */
    int iter_count = 0;
    uint32_t cur_val = 0;
    for (int row = prog_dim - 1; row >= 0; row--) {
        for (int col = 0; col < prog_dim; col++) {
            iter_count++;
            cur_val = (cur_val  << 4) | prog_bits[row][col];
            nibble_count++;
            /* printf("@[%d,%d] == %d, nib count %d, cur val %08X\n", row, col, prog_bits[row][col], nibble_count, cur_val); */

            if (nibble_count >= 8) {
                out_words[word_index] = cur_val;

                /* printf("--index %d, word %08X--\n", word_index, cur_val); */
                word_index--;
                out_words[word_index] = 0;

                cur_val = 0;
                nibble_count = 0;
            }
        }
    }
    /* printf("iter_count: %d, num_words: %d, word index: %d\n", iter_count, *num_words, word_index); */

    return out_words;
}

void cobi_prepare_weights(
    int **weights, int weight_dim, int shil_val, uint8_t *control_bits, int **program_array
) {
    if (weight_dim != COBI_WEIGHT_MATRIX_DIM) {
        printf("Bad weight dimensions: %d by %d\n", weight_dim, weight_dim);
        exit(2);
    }

    int mapped_shil_val = hex_mapping(shil_val);
    int program_dim = COBI_PROGRAM_MATRIX_DIM; // 52

    // zero outer rows and columns
    for(int k = 0; k < program_dim; k++) {
        program_array[0][k] = 0;
        program_array[1][k] = 0;

        program_array[k][0] = 0;
        program_array[k][1] = 0;

        program_array[k][program_dim-2] = 0;
        program_array[k][program_dim-1] = 0;

        program_array[program_dim-2][k] = 0;
        program_array[program_dim-1][k] = 0;
    }

    // add control bits in last row
    for(int k = 0; k < program_dim; k++) {
        program_array[program_dim-1][k] = control_bits[k];
    }

    // add shil
    for (int k = 1; k < program_dim - 1; k++) {
        // rows
        program_array[COBI_SHIL_ROW][k]     = mapped_shil_val;
        program_array[COBI_SHIL_ROW + 1][k] = mapped_shil_val;

        // columns
        program_array[k][COBI_SHIL_ROW - 2]     = mapped_shil_val;
        program_array[k][COBI_SHIL_ROW - 2 + 1] = mapped_shil_val;
    }

    // populate weights
    int row;
    int col;
    const int lfo_index = 1;
    for (int i = 0; i < weight_dim; i++) {
        if (i < COBI_SHIL_ROW - 2) {
            row = i + 2;
        } else {
            row = i + 4; // account for 2 shil rows and zeroed rows
        }

        for (int j = 0; j < weight_dim; j++) {
            if (j < COBI_SHIL_ROW - 2) {
                col = program_dim - 3 - j;
            } else {
                col = program_dim - 5 - j;
            }

            if (i == j) {
                int val = hex_mapping(weights[i][i] / 2);
                program_array[lfo_index][col] = val;
                program_array[row][lfo_index] = val;
            } else {
                program_array[row][col] = hex_mapping(weights[i][j]);
            }
        }
    }

    // diag
    for (int i = 0; i < program_dim; i++) {
        program_array[i][i] = 0;
    }
}

void cobi_wait_while_busy()
{
    uint32_t read_data;
    off_t read_offset;

    while(1) {
        read_offset = 10 * sizeof(uint32_t); // Example read offset

        // Write read offset to device
        if (write(cobi_fd, &read_offset, sizeof(read_offset)) != sizeof(read_offset)) {
            perror("Failed to set read offset in device");
            exit(2);
        }

        // Read data from device
        if (read(cobi_fd, &read_data, sizeof(read_data)) != sizeof(read_data)) {
            perror("Failed to read from device");
            exit(2);
        }

        if(read_data == 0x0){
            break;
        }
    }
}

#define RAW_BYTE_COUNT 338
void cobi_write_program(uint32_t *program, int num_words)
{
    pci_write_data write_data;

    if (num_words != (COBI_PROGRAM_MATRIX_DIM * COBI_PROGRAM_MATRIX_DIM / 8)) {
        perror("Bad program size\n");
        exit(-9);
    }

    if (Verbose_ > 3) {
        printf("Programming chip\n");
    }

    cobi_wait_while_busy();

    /* /\* Perform write operations *\/ */
    for (int i = 0; i < RAW_BYTE_COUNT; ++i) {
        write_data.offset = 9 * sizeof(uint32_t);
        write_data.value = program[i];    // data to write
        write_data.value = ((write_data.value & 0xFFFF0000) >> 16) |
            ((write_data.value & 0x0000FFFF) << 16);
        if (write(cobi_fd, &write_data, sizeof(write_data)) != sizeof(write_data)) {
            perror("Failed to write to device");
            close(cobi_fd);
            exit(-10);
        }
    }

    if (Verbose_ > 3) {
        printf("Programming completed\n");
    }
}

// Perform two's compliment conversion to int for a given bit sequence
int bits_to_signed_int(bool *bits, unsigned int num_bits)
{
    if (num_bits >= 8 * sizeof(int)) {
        perror("Bits cannot be represented as int");
        exit(2);
    }

    int n = 0;
    for (unsigned int i = 0; i < num_bits; i++) {
        n |= bits[i] << (num_bits - 1 - i);
    }

    unsigned int sign_bit = 1 << (num_bits - 1);
    return (n & (sign_bit - 1)) - (n & sign_bit);

    /* n = int(x, 2) */
    /* s = 1 << (bits - 1) */
    /* return (n & s - 1) - (n & s) */
}

void cobi_read(CobiData *cobi_data, bool use_polling)
{
    uint32_t read_data;
    off_t read_offset;
    int num_read_bits = 32;

    if (use_polling) {
        cobi_wait_while_busy();
    } else {
        usleep(350);
    }

    // Read from chip 1
    for (int i = 0; i < 3; ++i) {
        read_offset = (4 + i) * sizeof(uint32_t);

        // Write read offset to device
        if (write(cobi_fd, &read_offset, sizeof(read_offset)) != sizeof(read_offset)) {
            perror("Failed to set read offset in device");
            exit(-3);
        }

        // Read data from device
        if (read(cobi_fd, &read_data, sizeof(read_data)) != sizeof(read_data)) {
            perror("Failed to read from device");
            exit(-4);
        }

        // reverse order of bits
        if (i == 2) {
            num_read_bits = 14;
        } else {
            num_read_bits = 32;
        }
        for (int j = 0; j < num_read_bits; j++) {
            uint32_t mask = 1 << j;
            // cobi_data->chip1_output[32 * i + j] = ((read_data & mask) >> j) << (num_read_bits - 1 - j);
            cobi_data->chip1_output[32 * i + j] = ((read_data & mask) >> j);
        }

        if (Verbose_ > 2) {
            printf("Read data from cobi chip A at offset %ld: 0x%x\n", read_offset, read_data);
        }
    }

    // Read from chip 2

    for (int i = 0; i < 3; ++i) {
        // TODO verify read_offset calculations and consolidate with chip1 read loop

        read_offset = (13 + i) * sizeof(uint32_t);

        // Write read offset to device
        if (write(cobi_fd, &read_offset, sizeof(read_offset)) != sizeof(read_offset)) {
            perror("Failed to set read offset in device");
            exit(-5);
        }

        // Read data from device
        if (read(cobi_fd, &read_data, sizeof(read_data)) != sizeof(read_data)) {
            perror("Failed to read from device");
            exit(-6);
        }

        // reverse order of bits
        if (i == 2) {
            num_read_bits = 14;
        } else {
            num_read_bits = 32;
        }
        for (int j = 0; j < num_read_bits; j++) {
            uint32_t mask = 1 << j;
            //cobi_data->chip2_output[32 * i + j] = ((read_data & mask) >> j) << (num_read_bits - 1 - j);
            cobi_data->chip2_output[32 * i + j] = ((read_data & mask) >> j);
        }

        if (Verbose_ > 2) {
            printf("Read data from cobi chip B at offset %ld: 0x%x\n", read_offset, read_data);
        }
    }

    // Get process time

    read_offset = 48 * sizeof(uint32_t); // Example read offset

    if (write(cobi_fd, &read_offset, sizeof(read_offset)) != sizeof(read_offset)) {
        perror("Failed to set read offset in device");
        exit(-7);
    }

    if (read(cobi_fd, &read_data, sizeof(read_data)) != sizeof(read_data)) {
        perror("Failed to read from device");
        exit(-8);
    }

    cobi_data->process_time = read_data;

    if (Verbose_ > 2) {
        printf("Process time: 0x%x\n", read_data);
    }

    // Parse program id
    cobi_data->chip1_problem_id = 0;
    cobi_data->chip2_problem_id = 0;
    for (int i = 0; i < 12; i++) {
        cobi_data->chip1_problem_id =
            (cobi_data->chip1_problem_id << 1) | cobi_data->chip1_output[i];
        cobi_data->chip2_problem_id =
            (cobi_data->chip2_problem_id << 1) | cobi_data->chip2_output[i];
    }

    // Parse energy
    cobi_data->chip1_energy = bits_to_signed_int(&cobi_data->chip1_output[12], 16);
    cobi_data->chip2_energy = bits_to_signed_int(&cobi_data->chip2_output[12], 16);

    // Parse spins, from bits 28 to 74
    for (int i = 0; i < 46; i++) {
        cobi_data->chip1_spins[i] = cobi_data->chip1_output[i + 28];
        cobi_data->chip2_spins[i] = cobi_data->chip2_output[i + 28];
    }

    // Parse core id
    cobi_data->chip1_core_id = 0;
    cobi_data->chip2_core_id = 0;
    for (int i = 74; i < 78; i++) {
        cobi_data->chip1_core_id =
            (cobi_data->chip1_core_id << 1) | cobi_data->chip1_output[i];
        cobi_data->chip2_core_id =
            (cobi_data->chip2_core_id << 1) | cobi_data->chip2_output[i];
    }
}

void print_int_array2d(int **a, int w, int h)
{
    int x, y;
    for (x = 0; x < w; x++) {
        for (y = 0; y < h; y++) {
            printf("%d ", a[x][y]);
        }
        printf("\n");
    }
}

int *cobi_test_multi_times(
    CobiData *cobi_data, int sample_times, int size, int8_t *solution, bool use_polling
) {
    int times = 0;
    int *all_results = _malloc_array1d(sample_times);
    int cur_best = 0;
    int res;

    double reftime = 0;

    while (times < sample_times) {
        reftime = omp_get_wtime();
        cobi_write_program(cobi_data->serialized_program, cobi_data->serialized_len);
        write_cum_time += omp_get_wtime() - reftime;

        reftime = omp_get_wtime();
        cobi_read(cobi_data, use_polling);
        read_cum_time += omp_get_wtime() - reftime;

        if (Verbose_ > 2) {
            printf("\nSample number %d\n", times);
        }

        // TODO use results from both chips
        res = cobi_data->chip1_energy;

        all_results[times] = res;

        // Temporarily use 0xFF as the every problem id to verify we are getting results
        if (res == 0 && cobi_data->chip1_problem_id != 0xFF) {
            subprob_zero_count++;
            if (Verbose_ > 0) {
                printf("Subprob zero:\n");
                print_int_array2d(cobi_data->programming_bits, 52, 52);

                for (int i = 0; i < cobi_data->serialized_len; i++) {
                    printf("0X%08X,", cobi_data->serialized_program);
                    if (i % 7  == 6) {
                        printf("\n");
                    }
                }
            }
            continue;
        }

        if (res < cur_best) {
            cur_best = res;
            for (size_t i = 0; i < cobi_data->probSize; i++ ) {
                solution[i] = cobi_data->chip1_spins[i] == 0 ? 1 : -1;
            }
        }

        times += 1;

        if (Verbose_ > 2) {
            printf(", best = %d ",  cur_best);
        }
    }

    if (Verbose_ > 2) {
        printf("Finished!\n");
    }

    return all_results;
}

// Normalize
void cobi_norm_val(int **norm, double **ising, size_t size)
{
    // TODO consider alternate mapping/normalization schemes
    double min = 0;
    double max = 0;
    double cur_v = 0;

    size_t i,j;
    for (i = 0; i < size; i++) {
        for (j = 0; j < size; j++) {
            cur_v = ising[i][j];

            if (cur_v > max) max = cur_v;
            if (cur_v < min) min = cur_v;
        }
    }

    // Linear scaling to range [-14, 14]
    // (y + 14) / (x - min) = 28 / (max - min)
    // y = (28 / (max - min)) * (x - min) - 14

    const double upscale_factor = 1.5;
    const int top = 14;
    const int bot = -14;
    const int range = top - bot;

    for (i = 0; i < size; i++) {
        for (j = i; j < size; j++) {
            cur_v = ising[i][j];

            // linearly interpolate and upscale
            int scaled_v = round(
                upscale_factor * ((range * (cur_v - min) / (max - min)) - top)
            );

            // clamp
            if (scaled_v < 0 && scaled_v < bot) {
                scaled_v = bot;
            } else if (scaled_v > 0 && scaled_v > top) {
                scaled_v = top;
            }

            if (cur_v == 0) {
                norm[i][j] = 0;
                norm[j][i] = 0;
            } else if (i == j) {
                norm[i][i] = scaled_v;
            } else {
                int symmetric_val = round(scaled_v / 2);
                norm[i][j] = symmetric_val;
                norm[j][i] = symmetric_val;
            }
        }
    }
}

void ising_solution_from_qubo_solution(int8_t *ising_soln, int8_t *qubo_soln, int len)
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
void ising_from_qubo(double **ising, double **qubo, int size)
{
    for (int i = 0; i < size; i++) {
        ising[i][i] = qubo[i][i] / 2;

        for (int j = i + 1; j < size; j++) {
            // convert quadratic terms
            ising[i][j] = qubo[i][j] / 4;
            ising[j][i] = qubo[j][i] / 4;

            // add rest of linear term
            ising[i][i] += ising[i][j] + ising[j][i];
        }
    }
}

bool cobi_established(const char* device_file)
{
    if (access(device_file, F_OK) == 0) {
        if (Verbose_ > 0) printf("Accessing device file: %s\n", device_file);
        return true;
    } else {
        // fprintf(stderr, "No device found at %s\n", device_file);
        return false;
    }
}

int cobi_init()
{
    int nextdevno = 0;
    char device_file[22] = { 0 };

    do {
        snprintf(device_file, sizeof(device_file), DEVICE_FILE_TEMPLATE, nextdevno);
        if (cobi_established(device_file)) {
            if (Verbose_ > 2) {
                printf("cobi init: trying board %s\n", device_file);
            }

            cobi_fd = open(device_file, O_RDWR); // O_RDWR to allow both reading and writing
            nextdevno++;
        } else if (nextdevno > 0) {
            nextdevno = 0;
            if (Verbose_ > 2) {
                printf("cobi init: all cobi boards are busy.. waiting..\n");
            }
            usleep(1000); // all devices are currently busy, wait before trying again
        }
    } while (cobi_fd < 0);

    if (cobi_fd < 0) {
        perror("Failed to open device file");
        exit(2);
    }

    cobi_reset();

    if(Verbose_ > 0) {
        printf("Cobi chip initialized successfully\n");
    }

    return 0;
}

void cobi_solver(
    double **qubo, int numSpins, int8_t *qubo_solution, int num_samples,
    bool descend, bool use_polling
) {
    if (numSpins != COBI_WEIGHT_MATRIX_DIM) {
        printf("Quitting.. cobi_solver called with size %d. Cannot be greater than 46.\n", numSpins);
        exit(2);
    }

    CobiData *cobi_data = cobi_data_mk(numSpins, descend);
    int8_t *ising_solution = (int8_t*)malloc(sizeof(int8_t) * numSpins);
    double **ising = _malloc_double_array2d(numSpins, numSpins);
    int **norm_ising = _malloc_array2d(numSpins, numSpins);

    ising_from_qubo(ising, qubo, numSpins);
    cobi_norm_val(norm_ising, ising, numSpins);

    cobi_prepare_weights(
        norm_ising, COBI_WEIGHT_MATRIX_DIM, COBI_SHIL_VAL,
        default_control_nibbles, cobi_data->programming_bits
    );

    cobi_data->serialized_program = cobi_serialize_programming_bits(
        cobi_data->programming_bits, COBI_PROGRAM_MATRIX_DIM, &cobi_data->serialized_len
    );

    // Convert solution from QUBO to ising
    ising_solution_from_qubo_solution(ising_solution, qubo_solution, numSpins);

    int *results = cobi_test_multi_times(
        cobi_data, num_samples, numSpins, ising_solution, use_polling
    );

    // Convert ising solution back to QUBO form
    qubo_solution_from_ising_solution(qubo_solution, ising_solution, numSpins);

    free(results);
    free_cobi_data(cobi_data);
    free(ising_solution);
    _free_array2d((void**)ising, numSpins);
    _free_array2d((void**)norm_ising, numSpins);
}

// cobi_close should be registered by main to run via `atexit`
void cobi_close()
{
    if (Verbose_ > 0) {
        printf("cobi chip clean up\n");
    }

    // cobi_reset();
    close(cobi_fd);
}

#ifdef __cplusplus
}
#endif
