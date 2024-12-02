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

#define PCI_PROGRAM_LEN 166 // 166 * 64bits = (51 * 52 + 4(dummy bits)) * 4bits

#define COBI_DEVICE_NAME_LEN 22 // assumes 2 digit card number and null byte
#define COBI_DEVICE_NAME_TEMPLATE "/dev/cobi_pcie_card%hhu"

#define COBI_NUM_SPINS 45 // Doesn't count the local field spin
#define COBI_PROGRAM_MATRIX_HEIGHT 51
#define COBI_PROGRAM_MATRIX_WIDTH 52

#define COBI_SHIL_VAL 0
#define COBI_SHIL_INDEX 22 // index of first SHIL row/col in 2d program array

#define COBI_CHIP_OUTPUT_READ_COUNT 3 // number of reads needed to get a full output
#define COBI_CHIP_OUTPUT_LEN 69  // number of bits in a full output

#define COBI_CONTROL_NIBBLES_LEN 52

// FPGA

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

// -- Data structs --

typedef struct {
    off_t offset;
    uint64_t value;
} pci_write_data;

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
    uint64_t *serialized_program;
    int serialized_len;

    size_t num_outputs;
    CobiOutput **chip_output;

    uint32_t process_time;

    int sample_test_count;
} CobiData;

// -- End data structs --


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

// Perform 1's compliment conversion to int for a given bit sequence
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
    int x, y;
    for (x = 0; x < w; x++) {
        for (y = 0; y < h; y++) {
            printf("%2d ", a[x][y]);
        }
        printf("\n");
    }
}

void print_array2d_uint8(uint8_t **a, int w, int h)
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

// @param weights 2d array of ints values must be in range -7 to 7
// Assume program_array was already initialized to 0
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

    // populate weights
    int row;
    int col;
    const int lfo_index_col = 3;
    const int lfo_index_row = COBI_PROGRAM_MATRIX_HEIGHT - 3;
    for (int i = 0; i < COBI_NUM_SPINS; i++) {
        // the program matrix is flipped vertically relative to the weight matrix
        if (i < COBI_SHIL_INDEX) {
            row = COBI_PROGRAM_MATRIX_HEIGHT - 3 - i; // skip the control and calibration rows
        } else {
            row = COBI_PROGRAM_MATRIX_HEIGHT - 5 - i; // also skip the 2 shil rows
        }

        for (int j = 0; j < COBI_NUM_SPINS; j++) {
            if (j < COBI_SHIL_INDEX) {
                col = j + 3; // skip the 2 zero cols and calibration cols
            } else {
                col = j + 5; // also skip the 2 shil cols
            }

            if (i == j) {
                int split = weights[i][i] / 2;
                int val1 = hex_mapping(split);
                int val2 = hex_mapping(split + (weights[i][i] % 2));

                program_array[lfo_index_row][col] = val1;
                program_array[row][lfo_index_col] = val2;
                program_array[lfo_index_row][lfo_index_col] = 0; // diag
            } else {
                program_array[row][col] = hex_mapping(weights[i][j]);
            }
        }
    }
}

// Serialize the matrix to be programmed to the cobi chip
//
// @param prog_nibs The matrix to be programmed, assumed to be a 2d array of nibbles
// returns data in serialized array of 64 bit chunks
uint64_t *cobi_serialize_programming_bits(uint8_t **prog_nibs)
{
    uint64_t *serialized = (uint64_t*)malloc(PCI_PROGRAM_LEN * sizeof(uint64_t));

    int serial_index = 0;

    int cur_nib_count = 0;
    uint64_t cur_val = 0;

    for (int row = 0; row < COBI_PROGRAM_MATRIX_HEIGHT; row++) {
        for (int col = COBI_PROGRAM_MATRIX_WIDTH - 1; col >= 0; col--) {
            cur_val |= ((uint64_t) prog_nibs[row][col] << (4 * cur_nib_count));
            cur_nib_count++;

            if (cur_nib_count == 16) {
                serialized[serial_index++] = cur_val;
                cur_val = 0;
                cur_nib_count = 0;
            }
        }
    }

    if (cur_nib_count != 0) {
        // Add last 16 nib number
        serialized[serial_index++] = cur_val;
    }

    return serialized;
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
    read_offset = 10 * sizeof(uint32_t); // read fifo empty status

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

void cobi_read(int cobi_fd, CobiData *cobi_data, int num_to_read)
{
    uint32_t read_data;
    off_t read_offset;
    int num_read_bits = 32;

    for (int read_count = 0; read_count < num_to_read; read_count++) {
        read_data = cobi_read_status(cobi_fd);
        printf("status read: 0x%x\n", read_data);
        cobi_wait_for_read(cobi_fd);

        CobiOutput *output = cobi_data->chip_output[read_count];
        uint8_t bits[COBI_CHIP_OUTPUT_LEN] = {0};

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

        // Parse energy from last 15 bits
        output->energy = bits_to_signed_int(&bits[bit_index], 15);
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
}


// Write one serialized problem to the given cobi device
void cobi_write_program(int cobi_fd, uint64_t *program)
{
    pci_write_data write_data[PCI_PROGRAM_LEN];

    /* cobi_wait_for_write(cobi_fd); */

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
        printf("%16lX\n", program[x]);
        }
    printf("--\n");
}

typedef struct {
    int Q[46][46];
    int energy;
    int spins[46];
    int debug;
} nums;

void solveQ(nums* obj, int device) {
    char device_file[COBI_DEVICE_NAME_LEN];

    srand(time(NULL));

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

    // printf("Cobi chip initialized successfully\n");

    CobiData *cbd = cobi_data_mk(COBI_NUM_SPINS, 1);
    /* memset(cbd->chip1_output, 0, sizeof(uint8_t) * CHIP_OUTPUT_SIZE); */

    int** weights = _malloc_array2d(COBI_NUM_SPINS, COBI_NUM_SPINS);

    int i, j;
    for (i = 0; i < COBI_NUM_SPINS; i++) {
        for (j = 0; j < COBI_NUM_SPINS; j++) {
            weights[i][j] = obj->Q[i][j];
        }
    }

    cobi_prepare_weights(
        weights, COBI_SHIL_VAL, default_control_nibbles,
         cbd->programming_bits
    );

    uint64_t *data_words = cobi_serialize_programming_bits(cbd->programming_bits);

    // printf("46 by 46 weight matrix:\n");
    if(obj->debug) {
        printf("Matrix to be programmed:\n");
        print_array2d_uint8(cbd->programming_bits, COBI_PROGRAM_MATRIX_WIDTH, COBI_PROGRAM_MATRIX_HEIGHT);
    }

    if(obj->debug){
       printf("Serialization of programming bits:\n");
       /* print_program(data_words); */
       for (int i = 0; i < PCI_PROGRAM_LEN; i++) {
          printf("0X%08lX,", data_words[i]);
          if (i % 7  == 6) {
             printf("\n");
          }
       }
       printf("\n");
    }

    double startTime = omp_get_wtime();
    for (int i = 0; i < 1; i++) {
        cobi_write_program(cobi_fd, data_words);
    }
    /* usleep(100); */

    cobi_read(cobi_fd, cbd, 1);
    double finalTime = omp_get_wtime();

    // TMP
    CobiOutput *output = cbd->chip_output[0];

    if(obj->debug){
        printf("Wall time: %f\n", finalTime - startTime);
        printf("omp tick: %f\n", omp_get_wtick());
        // printf("Descend: %d\n", cbd->descend);

        printf("\nParsed output:\n--\n");
        printf("Problem id: %d\n", output->problem_id);
        printf("Core id: %d\n", output->core_id);

        printf("Energy: %d\n", output->energy);
        print_spins(output->spins);
    }

    for (int i=0; i<46; i++)
        obj->spins[i] = output->spins[i];
    obj->energy = output->energy;

    cobi_reset(cobi_fd);
    close(cobi_fd);

    free_cobi_data(cbd);
}

int main()
{
    srand(time(NULL));

    nums obj;
    obj.debug = 1;

    /* int **weights = _gen_rand_array2d(46,46); */
    int w = 46;
    int h = 46;

    int density_percentile = 25;
    for (int i = 0; i < h; i++) {
        for (int j = 0; j < w; j++) {
            if (rand() % 100 < density_percentile) {
                obj.Q[i][j] = _rand_int_normalized();
            } else if (i == j) {
                obj.Q[i][j] = _rand_int_normalized();
            } else {
                obj.Q[i][j] = 0;
            }

            if (rand() % 2 == 1) {
                obj.Q[i][j] *= -1;
            }
        }
    }

    printf("input:\n");
    for(int i = 0; i < h; i++) {
        for(int j = 0; j < w; j++) {
                printf("%2d ", obj.Q[i][j]);
        }
        printf("\n");
    }
    printf("--\n");

    solveQ(&obj, 0);

    printf("\noutput:\n");
    printf("energy: %d\n", obj.energy);
    print_spins(obj.spins);
    printf("--\n");
    return 0;
}

/* void main() */
/* { */
/*     nums obj; */
/*     obj.debug = 1; */

/*     /\* int **weights = _gen_rand_array2d(46,46); *\/ */
/*     int w = 46; */
/*     int h = 46; */
/*     for (int i = 0; i < w; i++) { */
/*         for (int j = 0; j < h; j++) { */
/*             obj.Q[i][j] = _rand_int_normalized(); */
/*         } */
/*     } */

/*     printf("input:\n"); */
/*     print_array2d(obj.Q, 46, 46); */
/*     printf("--\n"); */

/*     solveQ(&obj, 0); */

/*     printf("\noutput:\n"); */
/*     printf("energy: %d\n", obj.energy); */
/*     print_spins(obj.spins); */
/*     printf("--\n"); */
/* } */
