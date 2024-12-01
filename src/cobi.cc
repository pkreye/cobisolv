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

static int *cobi_fds;
static int cobi_num_open = 0;

uint8_t default_control_nibbles[COBI_CONTROL_NIBBLES_LEN] = {
    0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA,
    0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA,
    0, 0, 0xF, 0xF, // pid
    0, 0, 0, 0x5,   // dco_data
    0, 0, 0xF, 0xF, // sample_delay
    0, 0, 0x1, 0xF, //  max_fails
    0, 0, 0, 0x3,   // rosc_time
    0, 0, 0, 0xF,   // shil_time
    0, 0, 0, 0x3,   // weight_time
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

// Misc utility function
void _print_array1d_uint8(uint8_t *a, int len)
{
    for (int i = 0; i < len; i++) {
        printf("%X ", a[i]);
    }
    printf("\n");
}

void _print_array2d_int(int **a, int w, int h)
{
    int row, col;
    for (row = 0; row < h; row++) {
        printf("%02d: ", row);
        for (col = 0; col < w; col++) {
            printf("%d ", a[row][col]);
        }
        printf("\n");
    }
}

void _print_array2d_uint8(uint8_t **a, int w, int h)
{
    int row, col;
    for (row = 0; row < h; row++) {
        printf("%02d: ", row);
        for (col = 0; col < w; col++) {
            printf("%x ", a[row][col]);
        }
        printf("\n");
    }
}


void _print_program(uint64_t *program)
{
    printf("Serialized program (len %d):\n", PCI_PROGRAM_LEN);
    for (int x = 0; x < PCI_PROGRAM_LEN; x++) {
        printf("%16lX\n", program[x]);
        }
    printf("--\n");
}

void _fprint_array2d_int(int **a, int w, int h)
{
    int row, col;
    for (row = 0; row < h; row++) {
        printf("%02d: ", row);
        for (col = 0; col < w; col++) {
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

// FPGA Control interface
void cobi_reset(int cobi_id)
{
    pci_write_data write_data;

    // initialize axi interface control
    write_data.offset = COBI_FPGA_ADDR_CONTROL * sizeof(uint32_t);
    write_data.value = 0;
    if (Verbose_ > 2) {
        printf("Initialize cobi chips interface controller\n");
    }

    if (write(cobi_fds[cobi_id], &write_data, sizeof(write_data)) != sizeof(write_data)) {
        perror("Failed to write to device");
        exit(3);
    }
}

void cobi_output_clear(CobiOutput *o, size_t num_spins)
{
    for (size_t i = 0; i < num_spins; i++) {
        o->spins[i] = 0;
    }

    o->problem_id = -1;
    o->core_id    = -1;
    o->energy     = 0;
}

CobiOutput *cobi_output_mk_default(size_t num_spins)
{
    CobiOutput *output = (CobiOutput*) malloc(sizeof(CobiOutput));
    output->spins      = _malloc_array1d(num_spins);
    output->problem_id = -1;
    output->core_id    = -1;
    output->energy     = 0;
    return output;
}

CobiData *cobi_data_mk(size_t num_spins, size_t num_samples)
{
    CobiData *d = (CobiData*)malloc(sizeof(CobiData));
    d->probSize = num_spins;
    d->w = COBI_PROGRAM_MATRIX_WIDTH;
    d->h = COBI_PROGRAM_MATRIX_HEIGHT;
    d->programming_bits = _malloc_array2d_uint8(d->w, d->h);

    // prepare enough outputs to hold all samples
    d->num_outputs = num_samples;
    d->chip_output = (CobiOutput**) malloc(sizeof(CobiOutput*) * num_samples);
    for (size_t i = 0; i < num_samples; i++) {
        d->chip_output[i] = cobi_output_mk_default(num_spins);
    }

    return d;
}

void free_cobi_output(CobiOutput *o)
{
    free(o->spins);
    free(o);
}

void free_cobi_data(CobiData *d)
{
    _free_array2d((void**)d->programming_bits, d->h);
    free(d->serialized_program);

    for(size_t i = 0; i < d->num_outputs; i++) {
        free_cobi_output(d->chip_output[i]);
    }

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

// Serialize the matrix to be programmed to the cobi chip
//
// @param prog_nibs The matrix to be programmed, assumed to be a 2d array of nibbles
// returns data in serialized array of 64 bit chunks
uint64_t *cobi_serialize_programming_bits(uint8_t **prog_nibs)
{
    if (Verbose_ > 5) {
        printf("Serialized program into %d 64bit chunks\n", PCI_PROGRAM_LEN);
    }


    uint64_t *serialized = (uint64_t*)malloc(PCI_PROGRAM_LEN * sizeof(uint64_t));

    int serial_index = 0;

    int cur_nib_count = 0;
    uint64_t cur_val = 0;

    for (int row = 0; row < COBI_PROGRAM_MATRIX_HEIGHT; row++) {
        for (int col = COBI_PROGRAM_MATRIX_WIDTH - 1; col >= 0; col--) {
            cur_val |= ((uint64_t) prog_nibs[row][col] << (4 * cur_nib_count));
            cur_nib_count++;

            if (Verbose_ > 5) {
                printf("nib %x; cur_val[%d] %lx\n", prog_nibs[row][col], cur_nib_count-1, cur_val);
            }

            if (cur_nib_count == 16) {
                if (Verbose_ > 5) {
                    printf("Adding val %lx\n", cur_val);
                }
                serialized[serial_index++] = cur_val;
                cur_val = 0;
                cur_nib_count = 0;
            }
        }
    }
    if (Verbose_ > 5) {
        printf("adding nib cur_val %lx\n", cur_val);
    }

    if (cur_nib_count != 0) {
        // Add last 16 nib number
        serialized[serial_index++] = cur_val;
    }

    if (Verbose_ > 5) {
        printf("output:\n");
        for (int i = 0; i < PCI_PROGRAM_LEN; i++) {
            printf("0x%lX\n", serialized[i]);
        }
    }

    return serialized;
}


// @param weights 2d array of ints, values must be in range -7 to 7
// Assume program_array was already initialized to 0
void cobi_prepare_weights(
    int **weights, uint8_t shil_val, uint8_t *control_bits, uint8_t **program_array
) {
    int mapped_shil_val = hex_mapping(shil_val);

    if (Verbose_ > 3) {
        printf("==\n");
        printf("Preparing Program:\n");
        _print_array2d_uint8(
            program_array, COBI_PROGRAM_MATRIX_WIDTH, COBI_PROGRAM_MATRIX_HEIGHT
        );
        printf("==\n");
    }

    // initialize outer rows
    for(int k = 0; k < COBI_PROGRAM_MATRIX_WIDTH; k++) {
        // add control bits in last row
        program_array[COBI_PROGRAM_MATRIX_HEIGHT - 1][k] = control_bits[k];
    }

    // add shil cols
    for (int k = 1; k < COBI_PROGRAM_MATRIX_HEIGHT - 2; k++) {
        program_array[k][COBI_SHIL_INDEX + 3] = mapped_shil_val;
        program_array[k][COBI_SHIL_INDEX + 4] = mapped_shil_val;
    }

    // add shil rows
    for (int k = 3; k < COBI_PROGRAM_MATRIX_WIDTH - 1; k++) {
        program_array[COBI_PROGRAM_MATRIX_HEIGHT - COBI_SHIL_INDEX - 3][k] = mapped_shil_val;
        program_array[COBI_PROGRAM_MATRIX_HEIGHT - COBI_SHIL_INDEX - 4][k] = mapped_shil_val;
    }

    if (Verbose_ > 3) {
        printf("==\n");
        printf("Preparing Program: pre weights\n");
        _print_array2d_uint8(
            program_array, COBI_PROGRAM_MATRIX_WIDTH, COBI_PROGRAM_MATRIX_HEIGHT
        );
        printf("==\n");
    }

    // populate weights
    int row;
    int col;
    const int lfo_index_col = 3;
    const int lfo_index_row = COBI_PROGRAM_MATRIX_HEIGHT - 3;
    for (int i = 0; i < COBI_WEIGHT_MATRIX_DIM; i++) {
        // the program matrix is flipped vertically relative to the weight matrix
        if (i < COBI_SHIL_INDEX) {
            row = COBI_PROGRAM_MATRIX_HEIGHT - 3 - i; // skip the control and calibration rows
        } else {
            row = COBI_PROGRAM_MATRIX_HEIGHT - 5 - i; // also skip the 2 shil rows
        }

        for (int j = 0; j < COBI_WEIGHT_MATRIX_DIM; j++) {
            if (j < COBI_SHIL_INDEX) {
                col = j + 3; // skip the 2 zero cols and calibration cols
            } else {
                col = j + 5; // also skip the 2 shil cols
            }

            if (Verbose_ > 4) {
                printf("--i,j: %d, %d --> row,col: %d, %d\n",i,j,row,col);
            }

            if (i == j) {
                int split = weights[i][i] / 2;
                int val1 = hex_mapping(split);
                int val2 = hex_mapping(split + (weights[i][i] % 2));

                if (Verbose_ > 4) {
                    printf(
                        "diag input: %d,%d; output %d,%d,,%d,%d\n",
                        i,j, lfo_index_row, col, lfo_index_col, row
                    );
                }
                program_array[lfo_index_row][col] = val1;
                program_array[row][lfo_index_col] = val2;
                program_array[lfo_index_row][lfo_index_col] = 0; // diag
            } else {
                if (program_array[row][col] != 0) {
                    if (Verbose_ > 4) {
                        printf("overwriting (%d,%d): %d\n", row, col, program_array[row][col]);
                    }
                }
                program_array[row][col] = hex_mapping(weights[i][j]);
            }
        }
    }
}

void cobi_wait_for_write(int cobi_id)
{
    uint32_t read_data;
    off_t read_offset;

    int s_ready_low_count = 0;

    // Poll status bit

    while(1) {
        read_offset = COBI_FPGA_ADDR_STATUS * sizeof(uint32_t); // Example read offset

        // Write read offset to device
        if (write(cobi_fds[cobi_id], &read_offset, sizeof(read_offset)) != sizeof(read_offset)) {
            perror("Failed to set read offset in device");
            exit(2);
        }

        // Read data from device
        if (read(cobi_fds[cobi_id], &read_data, sizeof(read_data)) != sizeof(read_data)) {
            perror("Failed to read from device");
            exit(2);
        }

        if((read_data & COBI_FPGA_STATUS_MASK_S_READY) == COBI_FPGA_STATUS_VALUE_S_READY) {
            break;
        } else {
            // if (s_ready_low_count > 50) {
            //     printf("ERROR S_READY stuck low: status %x\n", read_data);
            //     exit(3);
            // }
            s_ready_low_count++;
            // printf("S_READY LOW; status %x\n", read_data);
        }
    }

    if( Verbose_ > 2 ) {
        printf("Done waiting for write: number of polls %d\n", s_ready_low_count);
    }
}

void cobi_wait_for_read(int cobi_id)
{
    uint32_t read_data;
    off_t read_offset;

    // Poll status bit

    while(1) {
        read_offset = COBI_FPGA_ADDR_STATUS * sizeof(uint32_t); // Example read offset

        // Write read offset to device
        if (write(cobi_fds[cobi_id], &read_offset, sizeof(read_offset)) != sizeof(read_offset)) {
            perror("Failed to set read offset in device");
            exit(2);
        }

        // Read data from device
        if (read(cobi_fds[cobi_id], &read_data, sizeof(read_data)) != sizeof(read_data)) {
            perror("Failed to read from device");
            exit(2);
        }

        // Check there is at least one result to be read
        if((read_data & COBI_FPGA_STATUS_MASK_READ_FIFO_EMPTY) !=
           COBI_FPGA_STATUS_MASK_READ_FIFO_EMPTY
          ) {
            break;
        }
    }
}

uint64_t swap_bytes(uint64_t val) {;

    return ((val >> 8)  & 0x00FF00FF00FF00FF) |
           ((val << 8)  & 0xFF00FF00FF00FF00) ;
}

void cobi_write_program(int cobi_id, uint64_t *program)
{
    pci_write_data write_data[PCI_PROGRAM_LEN];

    if (Verbose_ > 3) {
        printf("Programming chip\n");
    }

    // cobi_reset(cobi_id);
    cobi_wait_for_write(cobi_id);

    // Prepare write data

    for (int i = 0; i < PCI_PROGRAM_LEN; i++) {
        write_data[i].offset = COBI_FPGA_ADDR_WRITE * sizeof(uint64_t);
        write_data[i].value = swap_bytes(program[i]); // data to write
    }

    //  Perform write operations
    int bytes_to_write = PCI_PROGRAM_LEN * sizeof(pci_write_data);
    if (write(cobi_fds[cobi_id], write_data, bytes_to_write) != bytes_to_write)
    {
        fprintf(stderr, "Failed to write to device %d\n", cobi_id);
        close(cobi_fds[cobi_id]);
        exit(10);
    }

    if (Verbose_ > 3) {
        printf("Programming completed\n");
    }
}

// Perform two's compliment conversion to int for a given bit sequence
int bits_to_signed_int(uint8_t *bits, unsigned int num_bits)
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

void cobi_read(int cobi_id, CobiData *cobi_data, int num_to_read)
{
    uint32_t read_data;
    off_t read_offset;
    int num_read_bits = 32;

    for (int read_count = 0; read_count < num_to_read; read_count++) {
        cobi_wait_for_read(cobi_id);

        CobiOutput *output = cobi_data->chip_output[read_count];
        uint8_t bits[COBI_CHIP_OUTPUT_LEN] = {0};

        // Read one result from chip
        for (int i = 0; i < COBI_CHIP_OUTPUT_READ_COUNT; ++i) {
            read_offset = 4 * sizeof(uint32_t);

            // Write read offset to device
            if (write(cobi_fds[cobi_id], &read_offset, sizeof(read_offset)) != sizeof(read_offset)) {
                perror("Failed to set read offset in device");
                exit(-3);
            }

            // Read data from device
            if (read(cobi_fds[cobi_id], &read_data, sizeof(read_data)) != sizeof(read_data)) {
                perror("Failed to read from device");
                exit(-4);
            }

            if (i == COBI_CHIP_OUTPUT_READ_COUNT - 1) {
                // Final read should have only 5 bits
                num_read_bits = 5;
            } else {
                num_read_bits = 32;
            }

            for (int j = 0; j < num_read_bits; j++) {
                uint32_t mask = 1 << j;

                // combine data into bit array while reversing order of bits
                bits[32 * i + j] = ((read_data & mask) >> j);
            }

            if (Verbose_ > 2) {
                printf("Read data from cobi chip A at offset %ld: 0x%x\n", read_offset, read_data);
            }
        }

        // Parse bits into CobiOutput

        int bit_index = 0;

        // Parse program id
        output->problem_id = 0;
        for (int i = 3; i >= 0; i--) {
            output->problem_id |= bits[bit_index++] << i;
        }

        // Parse core id
        output->core_id = 0;
        for (int i = 3; i >= 0; i--) {
            output->core_id |= bits[bit_index++] << i;
        }

        // Parse spins, from bit 8 to bit 53
        for (int i = 0; i < 46; i++) {
            output->spins[i] = bits[bit_index++];
        }

        // if (Verbose_ > 1) {
        //     // bit_index == 4+4+46 == 54;
        //     printf("cobi_read::energy index 54 =? %d\n", bit_index);
        // }

        // Parse energy from last 15 bits
        output->energy = bits_to_signed_int(&bits[bit_index], 15);

        if (Verbose_ > 1) {
            // bit_index == 4+4+46 == 54;
            printf("cobi output: problem_id %d, core_id %d, energy %d, spins: [ ",
                   output->problem_id, output->core_id, output->energy);
            for (int i = 0; i < 46; i++) {
                printf("%d", output->spins[i]);
            }
            printf(" ]\n");
        }
    }
}

int *cobi_test_multi_times(
    int cobi_id, CobiData *cobi_data, int sample_times, int8_t *solution
) {
    int *all_results = _malloc_array1d(sample_times);

    int best = 0;
    int best_index = 0;
    int res = 0;

    double reftime = 0;

    // TODO program device repeatedly based on sample_times
    reftime = omp_get_wtime();

    // Program chip once for each desired sample
    for (int i = 0; i < sample_times; i++) {
        cobi_write_program(cobi_id, cobi_data->serialized_program);
    }

    write_cum_time += omp_get_wtime() - reftime;

    reftime = omp_get_wtime();
    cobi_read(cobi_id, cobi_data, sample_times); // TODO separate reading a single output from all
    read_cum_time += omp_get_wtime() - reftime;

    // TODO look at all output results
    for (int i = 0; i < sample_times; i++) {
        if (Verbose_ > 2) {
            printf("\nSample number %d\n", i);
        }

        CobiOutput *output = cobi_data->chip_output[i];
        res = output->energy;

        if (Verbose_ > 1) {
            printf("\nsubsample energy (card fd:%d), chip:%d\n", cobi_id, output->energy);
        }

        all_results[i] = res;

        // Temporarily use 0xFF as every problem id to verify we are getting results
        if (res == 0 && output->problem_id != 0xFF) {
            subprob_zero_count++;
            if (Verbose_ > 1) {
                printf("Subprob zero:\n");
                _print_array2d_uint8(cobi_data->programming_bits, COBI_PROGRAM_MATRIX_WIDTH, COBI_PROGRAM_MATRIX_HEIGHT);

                for (int i = 0; i < PCI_PROGRAM_LEN; i++) {
                    printf("0X%16lX,", *cobi_data->serialized_program);
                    if (i % 7  == 6) {
                        printf("\n");
                    }
                }
            }
            continue;
        }

        if (res < best) {
            best = res;
            best_index = i;
        }

        if (Verbose_ > 2) {
            printf(", best = %d ",  best);
        }
    }

    // Copy best result into solution
    for (size_t i = 0; i < cobi_data->probSize; i++) {
        // map binary spins to {-1,1} values
        solution[i] = cobi_data->chip_output[best_index]->spins[i] == 0 ? 1 : -1;
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
        if (Verbose_ > 1) printf("Accessing device file: %s\n", device_file);
        return true;
    } else {
        // fprintf(stderr, "No device found at %s\n", device_file);
        return false;
    }
}

int cobi_init(int *req_num_devices, int specific_card)
{
    int num_dev_available = 0;
    char glob_pattern[25] = { 0 };

    if (specific_card >= 0) {
        if (Verbose_ > 0) {
            printf("Using COBI card %d", specific_card);
        }
        *req_num_devices = 1;
        snprintf(glob_pattern, sizeof(glob_pattern), COBI_DEVICE_NAME_TEMPLATE, specific_card);
    } else {
        strncpy(glob_pattern, "/dev/cobi_*", sizeof(glob_pattern));
    }

    glob_t cobi_dev_glob;
    glob(glob_pattern, GLOB_NOSORT, NULL, &cobi_dev_glob);

    if (Verbose_ > 0) {
        printf("%d COBI devices requested\n", *req_num_devices);
        printf("%ld COBI devices found:\n", cobi_dev_glob.gl_pathc);

        for(size_t i = 0; i < cobi_dev_glob.gl_pathc; i++) {
            printf("%s\n", cobi_dev_glob.gl_pathv[i]);
        }
    }

    num_dev_available = cobi_dev_glob.gl_pathc;

    // non-positive number used to request all available devices
    if (*req_num_devices <= 0) {
        *req_num_devices = num_dev_available;
    } else {
        *req_num_devices = MIN(*req_num_devices, num_dev_available);
    }

    if (Verbose_ > 0) {
        printf("Using %d COBI devices\n", *req_num_devices);
    }

    GETMEM(cobi_fds, int, *req_num_devices);

    for(int cur_dev = 0; cur_dev < num_dev_available; cur_dev++) {
        char *cur_dev_file = cobi_dev_glob.gl_pathv[cur_dev];
            // device exists and we can try to open and claim it
            if (Verbose_ > 2) {
                printf("trying device %s\n", cur_dev_file);
            }

            int fd = open(cur_dev_file, O_RDWR);
            if (fd < 0) {
                if (Verbose_ > 1) {
                    printf("failed to open %s\n", cur_dev_file);
                }
            } else {
                if (Verbose_ > 1) {
                    printf("card %d, id %d\n", cur_dev, cobi_num_open);
                }

                cobi_fds[cobi_num_open++] = fd;

                if (cobi_num_open >= *req_num_devices) {
                    break;
                }
            }
        }

    // Update requested number of devices with the the number of devices we were able to open
    *req_num_devices = cobi_num_open;

    if(cobi_num_open == 0) {
        fprintf(stderr, "Failed to open a COBI device\n");
        return 1;
    }

    for(int i = 0; i < cobi_num_open; i++) {
        cobi_reset(i);
    }

    if(Verbose_ > 0) {
        printf("%d COBI devices initialized successfully\n", cobi_num_open);
    }

    globfree(&cobi_dev_glob);
    return 0;
}

void cobi_solver(
    CobiSubSamplerParams *params,
    double **qubo, int num_spins, int8_t *qubo_solution
) {
    if (num_spins != COBI_WEIGHT_MATRIX_DIM) {
        printf(
            "Quitting.. cobi_solver called with size %d. Cannot be greater than %d.\n",
            num_spins, COBI_WEIGHT_MATRIX_DIM
        );
        exit(2);
    }

    CobiData *cobi_data = cobi_data_mk(num_spins, params->num_samples);
    int8_t *ising_solution = (int8_t*)malloc(sizeof(int8_t) * num_spins);
    double **ising = _malloc_array2d_double(num_spins, num_spins);
    int **norm_ising = _malloc_array2d_int(num_spins, num_spins);

    ising_from_qubo(ising, qubo, num_spins);
    cobi_norm_val(norm_ising, ising, num_spins);

    uint8_t *cntrl_nibbles = mk_control_nibbles(
        params->cntrl_pid,
        params->cntrl_dco,
        params->cntrl_sample_delay,
        params->cntrl_max_fails,
        params->cntrl_rosc_time,
        params->cntrl_shil_time,
        params->cntrl_weight_time,
        params->cntrl_sample_time
    );

    if (Verbose_ > 3) {
        printf("--\n");
        _print_array1d_uint8(cntrl_nibbles, COBI_CONTROL_NIBBLES_LEN);

        printf("Normalized ising:\n");
        _print_array2d_int(norm_ising, COBI_WEIGHT_MATRIX_DIM, COBI_WEIGHT_MATRIX_DIM);
        printf("--\n");
    }

    cobi_prepare_weights(
        norm_ising, params->shil_val,
        cntrl_nibbles, cobi_data->programming_bits
    );

    if (Verbose_ > 3) {
        printf("Program:\n");
        _print_array2d_uint8(
            cobi_data->programming_bits, COBI_PROGRAM_MATRIX_WIDTH, COBI_PROGRAM_MATRIX_HEIGHT
        );
        printf("--\n");
    }

    cobi_data->serialized_program = cobi_serialize_programming_bits(
        cobi_data->programming_bits
    );

    if (Verbose_ > 3) {
        _print_program(cobi_data->serialized_program);
    }

    // Convert solution from QUBO to ising
    ising_solution_from_qubo_solution(ising_solution, qubo_solution, num_spins);

    int *results = cobi_test_multi_times(
        params->device_id, cobi_data, params->num_samples, ising_solution
    );

    // Convert ising solution back to QUBO form
    qubo_solution_from_ising_solution(qubo_solution, ising_solution, num_spins);

    free(results);
    free_cobi_data(cobi_data);
    free(ising_solution);
    _free_array2d((void**)ising, num_spins);
    _free_array2d((void**)norm_ising, num_spins);
}

// cobi_close should be registered by main to run via `atexit`
void cobi_close()
{
    if (Verbose_ > 0) {
        printf("cobi chip clean up\n");
    }

    // cobi_reset();
    for (int i = 0; i < cobi_num_open; i++) {
        close(cobi_fds[i]);
    }
}

#ifdef __cplusplus
}
#endif
