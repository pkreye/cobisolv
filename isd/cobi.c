#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <omp.h>

#include "cobi.h"

// -- Globals --

int cobi_fd = -1;

uint8_t default_control_nibbles[COBI_CONTROL_NIBBLES_LEN] = {
    0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA,
    0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA,
    0x0, 0x0, 0xF, 0xF, // pid
    // 0x8, 0x6, 0x6, 0x6, // pid
    0, 0, 0, 0,  // dco_data
    0, 0, 0xF, 0xF, // sample_delay
    0, 0, 1, 0xF,     //  max_fails
    0, 0, 0, 3,    // rosc_time
    0, 0, 0, 0xF,    // shil_time
    0, 0, 0, 3,    // weight_time
    0, 0, 0xF, 0xD    // sample_time
};

// -- End globals --


// --  Misc utility --

uint64_t swap_bytes(uint64_t val) {;

    return ((val >> 8)  & 0x00FF00FF00FF00FF) |
           ((val << 8)  & 0xFF00FF00FF00FF00) ;
}

// Perform two's compliment conversion to int for a given bit sequence
int bits_to_signed_int(uint8_t *bits, size_t num_bits)
{
    if (num_bits >= 8 * sizeof(int)) {
        perror("Bits cannot be represented as int");
        exit(2);
    }

    int n = 0;
    for (size_t i = 0; i < num_bits; i++) {
        n |= bits[i] << (num_bits - 1 - i);
    }

    unsigned int sign_bit = 1 << (num_bits - 1);
    return (n & (sign_bit - 1)) - (n & sign_bit);

    /* n = int(x, 2) */
    /* s = 1 << (bits - 1) */
    /* return (n & s - 1) - (n & s) */
}

void print_array2d(int **a, int w, int h)
{
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            printf("%2d ", a[y][x]);
        }
        printf("\n");
    }
}

void print_program_matrix(uint8_t a[][COBI_PROGRAM_MATRIX_WIDTH])
{
    int row, col;
    for (row = 0; row < COBI_PROGRAM_MATRIX_HEIGHT; row++) {
        printf("%02d: ", row);
        for (col = 0; col < COBI_PROGRAM_MATRIX_WIDTH; col++) {
            printf("%x ", a[row][col]);
        }
        printf("\n");
    }
}


void print_spins(int *spins)
{
    printf("Spins: ");
    for (int i = 0; i < 46; i++) {
        printf("%c ", spins[i] == -1 ? '-' : '+');

    }
    printf("\n");
}

void print_program(uint64_t *program)
{
    printf("Serialized program (len %d):\n", PCI_PROGRAM_LEN);
    for (int x = 0; x < PCI_PROGRAM_LEN; x++) {
        printf("%16lX, ", program[x]);
        if (x % 7 == 0) {
            printf("\n");
        }
    }
    printf("\n--\n");
}

int _rand_int_normalized()
{
// generate random int normalized to -7 to 7
    if (rand() % 2 == 1) {
        return rand() % 8;
    } else {
        return -1 * (rand() % 8);
    }
}

int **_malloc_array2d(int w, int h)
{
    int** a = malloc(sizeof(int *) * w);
    if (a == NULL) {
        fprintf(stderr, "Bad malloc %s %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
        exit(1);
    }

    int i, j;
    for (i = 0; i < w; i++) {
        a[i] = malloc(sizeof(int) * h);
        for (j = 0; j < h; j++) {
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

void _free_array2d(void **a, int h) {
    int i;
    for (i = 0; i < h; i++) {
        free(a[i]);
    }
    free(a);
}

int **_gen_rand_array2d(int w, int h)
{
    int** a = _malloc_array2d(w, h);

    int i, j;
    for (i = 0; i < w; i++) {
        for (j = 0; j < h; j++) {
            a[i][j] = _rand_int_normalized();
        }
    }

    return a;
}

// -- End misc utility --


// -- Cobi functions --

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

CobiOutput *cobi_output_mk_default(size_t num_spins)
{
    CobiOutput *output = (CobiOutput*) malloc(sizeof(CobiOutput));
    memset(output->spins, 0, sizeof(output->spins));
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
    d->serialized_program = (uint64_t*) malloc(sizeof(uint64_t) * PCI_PROGRAM_LEN);

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

// @param weights input 2d array of ints values must be in range -7 to 7.
// Assumed not to use main diagonal. Local field values should be encoded in first row/col.
// @param program_array output matrix. Assumes program_array was initialized to 0 to start.
void cobi_prepare_weights(
    int weights[46][46], uint8_t shil_val, uint8_t *control_bits, uint8_t program_array[][COBI_PROGRAM_MATRIX_WIDTH]
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

    // populate weights
    int row;
    int col;
    for (int i = 0; i < COBI_NUM_SPINS; i++) {
        // the program matrix is flipped vertically relative to the weight matrix
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

            if (i == j)  {
                program_array[row][col] = 0;
            } else {
                program_array[row][col] = hex_mapping(weights[i][j]);
            }
        }
    }
}

// Serialize the matrix to be programmed to the cobi chip
//
// @param prog_nibs The matrix to be programmed, assumed to be a 2d array of nibbles
// @param serialized output is stored as serialized array of 64 bit chunks
void cobi_serialize_programming_bits(
    uint8_t prog_nibs[][COBI_PROGRAM_MATRIX_WIDTH], uint64_t serialized[PCI_PROGRAM_LEN]
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

void cobi_wait_for_write(int cobi_fd)
{
    uint32_t read_data;
    off_t read_offset;

    // Poll status bit

    while(1) {
        read_offset = COBI_FPGA_ADDR_STATUS * sizeof(uint32_t); // Example read offset

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

        if((read_data & COBI_FPGA_STATUS_MASK_S_READY) == COBI_FPGA_STATUS_VALUE_S_READY) {
            break;
        }
    }
}

void cobi_wait_for_read(int cobi_fd)
{
    uint32_t read_data;
    off_t read_offset;

    // Poll status bit

    while(1) {
        read_offset = COBI_FPGA_ADDR_STATUS * sizeof(uint32_t); // Example read offset

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

        // Check there is at least one result to be read
        if((read_data & COBI_FPGA_STATUS_MASK_READ_FIFO_EMPTY) !=
           COBI_FPGA_STATUS_MASK_READ_FIFO_EMPTY
          ) {
            break;
        }
    }
}

uint32_t cobi_read_status(int fd)
{
    uint32_t read_data;
    off_t read_offset;
    read_offset = COBI_FPGA_ADDR_STATUS * sizeof(uint32_t); // read fifo empty status

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

bool cobi_has_result(int cobi_fd)
{
    uint32_t read_data = cobi_read_status(cobi_fd);

    // Check there is at least one result to be read
    bool has_result = (read_data & COBI_FPGA_STATUS_MASK_READ_FIFO_EMPTY) != COBI_FPGA_STATUS_MASK_READ_FIFO_EMPTY;
    return has_result;
}

int cobi_get_result_count(int cobi_fd)
{
    uint32_t read_data = cobi_read_status(cobi_fd);

    int read_count = (read_data & COBI_FPGA_STATUS_MASK_READ_COUNT) >> 4;
    return read_count;
}

// Read a single result from the given COBI device.
// If wait_for_result is set, block until a result becomes availble
int cobi_read(int cobi_fd, CobiOutput *result, bool wait_for_result)
{
    uint32_t read_data;
    off_t read_offset;
    int result_count = 0;

    bool result_ready = cobi_has_result(cobi_fd);

    if (!result_ready && wait_for_result) {
        cobi_wait_for_read(cobi_fd);
        result_ready = true;
    }

    if (result_ready) {
        uint8_t bits[COBI_CHIP_OUTPUT_LEN] = {0};
        int num_read_bits = 32;

        // Read one result from chip
        for (int i = 0; i < COBI_CHIP_OUTPUT_READ_COUNT; ++i) {
            read_offset = 4 * sizeof(uint32_t);

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
        }

        result_count++;

        // Parse bits into CobiOutput
        int bit_index = 0;

        // Parse program id
        result->problem_id = 0;
        for (int i = 3; i >= 0; i--) {
            result->problem_id |= bits[bit_index++] << i;
        }

        // Parse core id
        result->core_id = 0;
        for (int i = 3; i >= 0; i--) {
            result->core_id |= bits[bit_index++] << i;
        }

        // Parse spins, from bit index 8 to 53
        for (int i = 0; i < COBI_NUM_SPINS; i++) {
            // reverse order of spins
            result->spins[COBI_NUM_SPINS - 1 - i] = bits[bit_index++] == 0 ? 1 : -1;
        }

        // Parse energy from last 15 bits
        result->energy = bits_to_signed_int(&bits[bit_index], 15);

        // check if another result is ready
        result_ready = cobi_has_result(cobi_fd);
    }

    return result_count;
}

void cobi_flush_results(int cobi_fd)
{
    CobiOutput result;

    // Read out all results
    while(cobi_has_result(cobi_fd)) {
        cobi_read(cobi_fd, &result, true);
    }
}

void cobi_reset(int cobi_fd)
{
    pci_write_data write_data;
    write_data.offset = COBI_FPGA_ADDR_CONTROL * sizeof(uint32_t);
    write_data.value = 0;    // initialize axi interface control

    if (write(cobi_fd, &write_data, sizeof(write_data)) != sizeof(write_data)) {
        perror("Failed to write to device");
        close(cobi_fd);
        exit(2);
    }

    cobi_flush_results(cobi_fd);
}


// Write one serialized problem to the given cobi device.
// Blocks until device is ready for problem to be written.
void cobi_write_program(int cobi_fd, uint64_t *program)
{
    pci_write_data write_data[PCI_PROGRAM_LEN];

    cobi_wait_for_write(cobi_fd);

    // Prepare write data

    for (int i = 0; i < PCI_PROGRAM_LEN; i++) {
        write_data[i].offset = COBI_FPGA_ADDR_WRITE * sizeof(uint64_t);
        write_data[i].value = swap_bytes(program[i]); // data to write
    }

    //  Perform write operations
    int bytes_to_write = PCI_PROGRAM_LEN * sizeof(pci_write_data);
    if (write(cobi_fd, write_data, bytes_to_write) != bytes_to_write)
    {
        fprintf(stderr, "Failed to write to device %d\n", cobi_fd);
        close(cobi_fd);
        exit(10);
    }
}

int cobi_open_device(int device)
{
    char device_file[COBI_DEVICE_NAME_LEN];

    snprintf(device_file, COBI_DEVICE_NAME_LEN, COBI_DEVICE_NAME_TEMPLATE, device);

    if (access(device_file, F_OK) == 0) {
        // printf("Accessing device file: %s\n", device_file);
    } else {
        fprintf(stderr, "No device found at %s\n", device_file);
        exit(1);
    }

    // pci_write_data write_data;
    cobi_fd = open(device_file, O_RDWR); // O_RDWR to allow both reading and writing
    if (cobi_fd < 0) {
        perror("Failed to open device file");
        exit(2);
    }

    return cobi_fd;
}

// Functions intended to be exported by python wrapper

void init_device(int device)
{
    int fd = cobi_open_device(device);
    cobi_reset(fd);
    close(fd);
}

void submit_problem(int device, CobiInput *obj)
{
    uint8_t program_matrix[COBI_PROGRAM_MATRIX_HEIGHT][COBI_PROGRAM_MATRIX_WIDTH] = {0};
    uint64_t serialized_program[PCI_PROGRAM_LEN] = {0};

    cobi_prepare_weights(
        obj->Q, COBI_SHIL_VAL, default_control_nibbles,
        program_matrix
    );

    cobi_serialize_programming_bits(program_matrix, serialized_program);

    if(obj->debug) {
        printf("Matrix to be programmed:\n");
        print_program_matrix(program_matrix);
        print_program(serialized_program);
    }

    int cobi_fd = cobi_open_device(device);
    cobi_write_program(cobi_fd, serialized_program);
    close(cobi_fd);
}

// Read a single result if one is available.
int read_result(int device, CobiOutput *result, bool wait_for_result)
{
    int cobi_fd = cobi_open_device(device);
    int result_count = cobi_read(cobi_fd, result, wait_for_result);
    close(cobi_fd);

    return result_count;
}

void solveQ(int device, CobiInput* obj, CobiOutput *result)
{
    uint8_t program_matrix[COBI_PROGRAM_MATRIX_HEIGHT][COBI_PROGRAM_MATRIX_WIDTH] = {0};
    uint64_t serialized_program[PCI_PROGRAM_LEN] = {0};

    double startTime = omp_get_wtime();

    // Prepare problem

    cobi_prepare_weights(
        obj->Q, COBI_SHIL_VAL, default_control_nibbles, program_matrix
    );

    cobi_serialize_programming_bits(program_matrix, serialized_program);

    if(obj->debug) {
        printf("Matrix to be programmed:\n");
        print_program_matrix(program_matrix);
        print_program(serialized_program);
    }

    // Open device

    int cobi_fd = cobi_open_device(device);
    cobi_reset(cobi_fd);

    // Write problem to device

    for (int i = 0; i < 1; i++) {
        cobi_write_program(cobi_fd, serialized_program);
    }

    /* submit_problem(obj, device); */

    // Wait for and read output
    int result_count = cobi_read(cobi_fd, result, true);
    double finalTime = omp_get_wtime();

    if(obj->debug){
        /* int num_to_read = cobi_get_result_count(cobi_fd); */
        /* printf("Read FIFO count: %d\n", num_to_read); */
        printf("%d results read\n", result_count);

        printf("Wall time: %f\n", finalTime - startTime);
        /* printf("omp tick: %f\n", omp_get_wtick()); */

        printf("\nParsed output:\n--\n");
        printf("Problem id: %d\n", result->problem_id);
        printf("Core id: %d\n", result->core_id);
        printf("Energy: %d\n", result->energy);
        printf("--\n");
        print_spins(result->spins);
    }

    // Clean up

    close(cobi_fd);
}
