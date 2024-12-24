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

// Local declarations
int cobi_read(int cobi_id, CobiOutput *output, bool wait_for_result);



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
    // free(d->serialized_program);

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

// FPGA interface functions

// Since firmware ID needs to be checked during init, pass the fd directly
uint32_t _cobi_get_fw_id(int fd)
{
    uint32_t read_data;
    off_t read_offset;
    read_offset = COBI_FPGA_ADDR_FW_ID * sizeof(uint32_t); // read fifo empty status

    // Write read offset to device
    if (write(fd, &read_offset, sizeof(read_offset)) != sizeof(read_offset)) {
        perror("Failed to set read offset in device");
        close(fd);
        exit(1);
    }

    // Read data from device
    if (read(fd, &read_data, sizeof(read_data)) != sizeof(read_data)) {
        perror("Failed to read from device");
        close(fd);
        exit(1);
    }

    /* printf("COBIFIVE status succeeding read: 0x%x\n", read_data); */
    return read_data;
}

uint32_t cobi_read_status(int cobi_id)
{
    uint32_t read_data;
    off_t read_offset;
    read_offset = COBI_FPGA_ADDR_STATUS * sizeof(uint32_t); // read fifo empty status

    // Write read offset to device
    if (write(cobi_fds[cobi_id], &read_offset, sizeof(read_offset)) != sizeof(read_offset)) {
        perror("Failed to set read offset in device");
        close(cobi_fds[cobi_id]);
        exit(1);
    }

    // Read data from device
    if (read(cobi_fds[cobi_id], &read_data, sizeof(read_data)) != sizeof(read_data)) {
        perror("Failed to read from device");
        close(cobi_fds[cobi_id]);
        exit(1);
    }

    /* printf("COBIFIVE status succeeding read: 0x%x\n", read_data); */
    return read_data;
}

bool cobi_has_result(int cobi_id)
{
    uint32_t read_data = cobi_read_status(cobi_id);

    // Check there is at least one result to be read
    bool has_result =
        (read_data & COBI_FPGA_STATUS_MASK_READ_FIFO_EMPTY) != COBI_FPGA_STATUS_MASK_READ_FIFO_EMPTY;
    return has_result;
}

void cobi_flush_results(int cobi_id)
{
    CobiOutput *result = cobi_output_mk_default(COBI_NUM_SPINS);

    // Read out all results
    while(cobi_has_result(cobi_id)) {
        cobi_read(cobi_id, result, true);
    }

    free_cobi_output(result);
}

void cobi_reset(int cobi_id)
{
    pci_write_data write_data;

    // initialize axi interface control
    write_data.offset = COBI_FPGA_ADDR_CONTROL * sizeof(uint64_t);
    write_data.value = 0;
    if (Verbose_ > 2) {
        printf("Initialize cobi chips interface controller\n");
    }

    if (write(cobi_fds[cobi_id], &write_data, sizeof(write_data)) != sizeof(write_data)) {
        perror("Failed to write to device");
        exit(3);
    }

    cobi_flush_results(cobi_id);
}


// Serialize the matrix to be programmed to the cobi chip
//
// @param prog_nibs The matrix to be programmed, assumed to be a 2d array of nibbles
// @param serialized output is stored as serialized array of 64 bit chunks
void cobi_serialize_programming_bits(
    uint8_t **prog_nibs, uint64_t serialized[PCI_PROGRAM_LEN]
) {
    int serial_index = 0;

    int cur_nib_count = 0;
    uint64_t cur_val = 0;

    cur_nib_count = 4; // account for padding to align with a multiple of 64

    // Iterate through matrix in reverse row order
    for (int row = COBI_PROGRAM_MATRIX_HEIGHT - 1; row >= 0; row--) {

        for (int col = 0; col < COBI_PROGRAM_MATRIX_WIDTH; col++) {
            cur_val = (cur_val << 4) | ((uint64_t) prog_nibs[row][col]);
            cur_nib_count++;

            if (cur_nib_count == 16) {
                // Build serial array in reverse
                serialized[PCI_PROGRAM_LEN - 1 - serial_index] = cur_val;
                serial_index++;
                cur_val = 0;
                cur_nib_count = 0;
            }
        }
    }
}

// @param weights input 2d array of ints, values must be in range -7 to 7.
// Main diagonal encodes local field values and can be in range -14 to 14.
// @param program_array output array. Assumed to be initialized to 0.
//
void cobi_prepare_weights(
    int **weights, uint8_t shil_val, uint8_t *control_bits, uint8_t **program_array
) {
    int mapped_shil_val = hex_mapping(shil_val);

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

    // if (Verbose_ > 3) {
    //     printf("==\n");
    //     printf("Preparing Program: pre weights\n");
    //     _print_array2d_uint8(
    //         program_array, COBI_PROGRAM_MATRIX_WIDTH, COBI_PROGRAM_MATRIX_HEIGHT
    //     );
    //     printf("==\n");
    // }

    // populate weights
    int row;
    int col;
    const int lfo_index_row = 1;
    const int lfo_index_col = COBI_PROGRAM_MATRIX_WIDTH - 2;
    for (int i = 0; i < COBI_NUM_SPINS; i++) {
        // NOTE: the program matrix is reflected horizontally relative to the weight matrix

        if (i < COBI_SHIL_INDEX + 2) {
            row = i + 1; // skip the calibration row
        } else {
            row = i + 3; // also skip the 2 shil rows
        }

        for (int j = 0; j < COBI_NUM_SPINS; j++) {
            if (j < COBI_SHIL_INDEX + 2) {
                col = COBI_PROGRAM_MATRIX_WIDTH - 2 - j; // skip the calibration col
            } else {
                col = COBI_PROGRAM_MATRIX_WIDTH - 4 - j; // also skip the 2 shil cols
            }

            if (i == j) {
                int split = weights[i][i] / 2;
                int val1 = hex_mapping(split);
                int val2 = hex_mapping(split + (weights[i][i] % 2));

                program_array[lfo_index_row][col] = val1;
                program_array[row][lfo_index_col] = val2;
                program_array[row][col] = 0; // zero diagonal
            } else {
                program_array[row][col] = hex_mapping(weights[i][j]);
            }
        }
    }
}

void cobi_wait_for_write(int cobi_id)
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

        if((read_data & COBI_FPGA_STATUS_MASK_S_READY) == COBI_FPGA_STATUS_VALUE_S_READY) {
            break;
        }
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

// Read a single result from the given COBI device.
// If wait_for_result is set, block until a result becomes availble
int cobi_read(int cobi_id, CobiOutput *output, bool wait_for_result)
{
    uint32_t read_data;
    off_t read_offset;
    int result_count = 0;

    bool result_ready = cobi_has_result(cobi_id);

    if (!result_ready && wait_for_result) {
        cobi_wait_for_read(cobi_id);
        result_ready = true;
    }

    if (result_ready) {
        uint8_t bits[COBI_CHIP_OUTPUT_LEN] = {0};
        int num_read_bits = 32;

        // Read one result from chip
        for (int i = 0; i < COBI_CHIP_OUTPUT_READ_COUNT; ++i) {
            read_offset = COBI_FPGA_ADDR_READ * sizeof(uint32_t);

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
            // reverse order of spins
            output->spins[COBI_NUM_SPINS - 1 - i] = bits[bit_index++] == 0 ? 1 : -1;
            // output->spins[i] = bits[bit_index++] == 0 ? 1 : -1;
        }

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

    return result_count;
}

int *cobi_test_multi_times(
    int cobi_id, CobiData *cobi_data, uint64_t *serialized_program, int sample_times, int8_t *solution
) {
    int *all_results = _malloc_array1d(sample_times);

    int best = 0;
    int best_index = 0;
    int res = 0;

    double reftime = 0;

    reftime = omp_get_wtime();

    // Program chip once for each desired sample
    for (int i = 0; i < sample_times; i++) {
        cobi_write_program(cobi_id, serialized_program);
    }

    write_cum_time += omp_get_wtime() - reftime;

    reftime = omp_get_wtime();
    for(int i = 0; i < sample_times; i++) {
        cobi_read(cobi_id, cobi_data->chip_output[i], true);
    }
    read_cum_time += omp_get_wtime() - reftime;

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
                    printf("0X%16lX,", serialized_program[i]);
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
        solution[i] = cobi_data->chip_output[best_index]->spins[i];
    }

    if (Verbose_ > 2) {
        printf("Finished!\n");
    }

    return all_results;
}

// Normalization schemes
// void cobi_norm_scaled(int **norm, double **ising, size_t size, double scale)
// {
//     const int WEIGHT_MAX = 14;
//     const int WEIGHT_MIN = -14;
//     for (size_t i = 0; i < size; i++) {
//         for (size_t j = i; j < size; j++) {
//             double cur_val = ising[i][j];
//             if (cur_val == 0) {
//                 norm[i][j] = 0;
//                 norm[j][i] = 0;
//                 continue;
//             }
//             // Non-zero case
//             int scaled_val = round(scale * cur_val);
//             if (cur_val > 0) {
//                 scaled_val = MIN(scaled_val, WEIGHT_MAX);
//             } else {
//                 scaled_val = MAX(scaled_val, WEIGHT_MIN);
//             }
//             if (i == j) {
//                 norm[i][i] = scaled_val;
//             } else {
//                 int symmetric_val = scaled_val / 2;
//                 norm[i][j] = symmetric_val;
//                 norm[j][i] = symmetric_val + (scaled_val % 2);
//             }
//         }
//     }
// }

void cobi_norm_scaled(int **norm, double **ising, size_t size, double scale)
{
    // compress to avoid known hw bug
    const int WEIGHT_MAX = 12;
    const int WEIGHT_MIN = -12;
    for (size_t i = 0; i < size; i++) {
        for (size_t j = i; j < size; j++) {
            double cur_val = ising[i][j];
            if (cur_val == 0) {
                norm[i][j] = 0;
                norm[j][i] = 0;
                continue;
            }

            // Non-zero case
            int scaled_val = round(scale * cur_val);
            if (cur_val > 0) {
                scaled_val = MIN(scaled_val, WEIGHT_MAX);
            } else {
                scaled_val = MAX(scaled_val, WEIGHT_MIN);
            }

            if (i == j) {
                norm[i][i] = scaled_val;

                // if (norm[i][i] > 0) {
                //     norm[i][i]--;
                // } else {
                //     norm[i][i]++;
                // }
            } else {
                int symmetric_val = scaled_val / 2;
                norm[i][j] = symmetric_val;
                norm[j][i] = symmetric_val + (scaled_val % 2);

                // // compress to avoid known hw bug
                // if (norm[i][j] > 0) {
                //     norm[i][j]--;
                //     norm[j][i]--;
                // } else {
                //     norm[i][j]++;
                //     norm[j][i]++;
                // }

            }
        }
    }
}

void cobi_norm_nonlinear(int **norm, double **ising, size_t size, double scale)
{
    const int WEIGHT_MAX = 14;
    const int WEIGHT_MIN = -14;
    for (size_t i = 0; i < size; i++) {
        for (size_t j = i; j < size; j++) {
            double cur_val = ising[i][j];
            if (cur_val == 0) {
                norm[i][j] = 0;
                norm[j][i] = 0;
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
                    norm[i][i] = scaled_val;
                } else {
                    int symmetric_val = scaled_val / 2;
                    norm[i][j] = symmetric_val;
                    norm[j][i] = symmetric_val + (scaled_val % 2);
                }
            }
        }
    }
}

void cobi_norm_linear(int **norm, double **ising, size_t size, double scale)
{
    double min = 0;
    double max = 0;
    double cur_v = 0;

    for (size_t i = 0; i < size; i++) {
        for (size_t j = i; j < size; j++) {
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

    for (size_t i = 0; i < size; i++) {
        for (size_t j = i; j < size; j++) {
            cur_v = ising[i][j];

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
                norm[i][j] = 0;
                norm[j][i] = 0;
            } else if (i == j) {
                norm[i][i] = scaled_val;
            } else {
                int symmetric_val = scaled_val / 2;
                norm[i][j] = symmetric_val;
                norm[j][i] = symmetric_val + (scaled_val % 2);
            }
        }
    }
}

void cobi_norm_val(
    int **norm_ising, double **ising, size_t num_spins, CobiEvalStrat strat, int prob_num
) {
    switch (strat) {
    case COBI_EVAL_NORM_LINEAR: {
        const double scales[4] = {0.5, 1, 2, 4};
        cobi_norm_linear(norm_ising, ising, num_spins, scales[prob_num]);
        break;
    }
    case COBI_EVAL_NORM_SCALED: {
        const double scales[4] = {0.5, 1, 2, 4};
        cobi_norm_scaled(norm_ising, ising, num_spins, scales[prob_num]);
        break;
    }
    case COBI_EVAL_NORM_NONLINEAR: {
        const double scales[4] = {2, 1, 0.5, 0.1};
        cobi_norm_nonlinear(norm_ising, ising, num_spins, scales[prob_num]);
        break;
    }
    case COBI_EVAL_NORM_MIXED: {
        switch (prob_num) {
        case 0:
            cobi_norm_nonlinear(norm_ising, ising, num_spins, 1);
            break;
        case 1:
            cobi_norm_linear(norm_ising, ising, num_spins, 1);
            break;
        case 2:
            cobi_norm_scaled(norm_ising, ising, num_spins, 2);
            break;
        case 3:
            cobi_norm_nonlinear(norm_ising, ising, num_spins, 2);
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
        cobi_norm_linear(norm_ising, ising, num_spins, 1);
        break;
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
            printf("Using COBI card %d\n", specific_card);
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
            // Verify expected firmware
            uint32_t fwid = _cobi_get_fw_id(fd);
            if (fwid == COBIFIVE_FW_ID) {
                cobi_fds[cobi_num_open++] = fd;

                if (cobi_num_open >= *req_num_devices) {
                    break;
                }
            } else if (Verbose_ > 1) {
                printf("COBI device has unexpected firmware id: 0x%x\n", fwid);
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
    CobiSubSamplerParams *params, double **qubo, int num_spins, int8_t *qubo_solution
) {
    if (num_spins != COBI_NUM_SPINS) {
        printf(
            "Quitting.. cobi_solver called with size %d. Cannot be greater than %d.\n",
            num_spins, COBI_NUM_SPINS
        );
        exit(2);
    }

    double reftime = 0;
    int best_energy = 0;
    int best_output = 0;

    // TODO locally allocate/manage arrays for improved performance

    int8_t *ising_solution = (int8_t*)malloc(sizeof(int8_t) * num_spins);
    double **ising = _malloc_array2d_double(num_spins, num_spins);
    int **norm_ising = _malloc_array2d_int(num_spins, num_spins);

    int num_probs; // to be run in parallel

    if (params->eval_strat == COBI_EVAL_SINGLE) {
        num_probs = 1;
    } else {
        num_probs = 4;
    }

    // TODO move away from using CobiData. Doesn't make much sense any more.
    CobiData *cobi_data = cobi_data_mk(num_spins, num_probs);

    // Convert problem from QUBO to Ising
    ising_from_qubo(ising, qubo, num_spins);

    // Convert solution from QUBO to Ising
    ising_solution_from_qubo_solution(ising_solution, qubo_solution, num_spins);

    for (int prob_num = 0; prob_num < num_probs; prob_num++) {

        // TODO shouldn't need to zero matrix every time. Being safe for now.
        zero_array2d(norm_ising, num_spins, num_spins);
        zero_array2d_uint8(
            cobi_data->programming_bits, COBI_PROGRAM_MATRIX_WIDTH, COBI_PROGRAM_MATRIX_HEIGHT
        );
        uint64_t serialized_program[PCI_PROGRAM_LEN] = {0};

        cobi_norm_val(norm_ising, ising, num_spins, params->eval_strat, prob_num);

        uint8_t *cntrl_nibbles = mk_control_nibbles(
            prob_num,
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
            _print_array2d_int(norm_ising, COBI_NUM_SPINS, COBI_NUM_SPINS);
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

        cobi_serialize_programming_bits(
            cobi_data->programming_bits, serialized_program
        );

        if (Verbose_ > 3) {
            _print_program(serialized_program);
        }

        reftime = omp_get_wtime();
        cobi_write_program(params->device_id, serialized_program);
        write_cum_time += omp_get_wtime() - reftime;
        // int *results = cobi_test_multi_times(
        //     params->device_id, cobi_data, params->num_samples, ising_solution
        // );

        free(cntrl_nibbles);
    }


    // Read out results
    reftime = omp_get_wtime();
    for (int i = 0; i < num_probs; i++) {
        cobi_read(params->device_id, cobi_data->chip_output[i], true);
        if (cobi_data->chip_output[i]->energy < best_energy) {
            best_energy = cobi_data->chip_output[i]->energy;
            best_output = i;
        }
    }
    read_cum_time += omp_get_wtime() - reftime;

    if (Verbose_ > 1) {
        printf("TEST::Best problem: %d, energy %d\n",
               cobi_data->chip_output[best_output]->problem_id,
               cobi_data->chip_output[best_output]->energy
              );
    }

    // Copy best result into solution
    for (size_t i = 0; i < cobi_data->probSize; i++) {
        ising_solution[i] = cobi_data->chip_output[best_output]->spins[i];
    }

    // Convert ising solution back to QUBO form
    qubo_solution_from_ising_solution(qubo_solution, ising_solution, num_spins);

    // free(results);
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

#ifdef __cplusplus
}
#endif
