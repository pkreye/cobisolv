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


#define RAW_BYTE_COUNT 338
#define SHIL_ROW 26
#define CHIP_OUTPUT_SIZE 78 // 32 + 32 + 14
#define COBI_CONTROL_NIBBLES_LEN 52
#define COBI_CHIP_OUTPUT_LEN 78

// -- Data structs --

typedef struct {
    off_t offset;
    uint32_t value;
} pci_write_data;

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

    int num_samples;
    int64_t chip_delay;
    bool descend;

    int sample_test_count;
} CobiData;

// -- End data structs --


// --  Misc utility --

// Perform 1's compliment conversion to int for a given bit sequence
int bits_to_signed_int(bool *bits, int num_bits)
{
    if (num_bits >= 8 * sizeof(int)) {
        perror("Bits cannot be represented as int");
        exit(2);
    }

    int n = 0;
    for (int i = 0; i < num_bits; i++) {
        n |= bits[i] << num_bits - 1 - i;
    }

    unsigned int sign_bit = 1 << (num_bits - 1);
    return (n & sign_bit - 1) - (n & sign_bit);

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


int _rand_int_normalized()
{
// generate random int normalized to -14 to 14
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

void _free_array2d(int **a, int w) {
    int i;
    for (i = 0; i < w; i++) {
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

/* uint32_t  rawData[343] = { */
/* 0X00000000,0X00000000,0X00000000,0X00000000,0X00000000,0X00000000,0X00000000, */
/* 0X00000000,0X00000000,0XFF000000,0X00000000,0X00000000,0X00000000,0X5FB65000, */
/* 0X540B99C0,0XD2ABB57B,0X90BCFFD7,0X2D70D6EA,0XADD57B20,0X000000D0,0XFA2EBDDE, */
/* 0XD5AB006C,0XFFE506E9,0XDED72BFF,0XE8CD2065,0X00A6F20B,0X43709200,0X7FDAAEFD, */
/* 0XE02E2786,0X280AFF2B,0X52758BB7,0X30034003,0XA000004D,0XE082DA07,0X4B6D82F0, */
/* 0XFF52BCB2,0X07342270,0X96E3A22A,0X00D5D487,0X0000C900,0X8F0AD30E,0XE53890ED, */
/* 0X2D09FFB4,0X0326E0FF,0X348D4CB9,0X6D0000C7,0X2C4F0F05,0X4D24460C,0XFF3D6672, */
/* 0XF4060F22,0X206EF93D,0X00A7092D,0X4AECC500,0X09BB4230,0X9866F58B,0XB49EFFB7, */
/* 0X02EA69AC,0X08BEE24F,0X600000A0,0X580338A7,0X28F8C4B0,0XFFD06669,0X7986500C, */
/* 0X7D3D25EA,0X008C4270,0X86395400,0XB9A0603E,0XF563E3BB,0XA2F8FF0A,0XAE5D0836, */
/* 0XCEBC986B,0XFA00007A,0X030B5084,0XC00A4300,0XFF36F664,0X543672B9,0XB8FBB792, */
/* 0X005A0F88,0X0DFC5400,0XC750F285,0X9DB55A27,0X2264FFEA,0XCB2550D0,0X0373F786, */
/* 0X3A0000A9,0XAABAA087,0X8E9E580E,0XFF04A760,0X8040DABD,0XCB9AD329,0X0070E623, */
/* 0XBD30E000,0X808A7B75,0XF63C5FBB,0X5988FFA0,0X4AA00ACB,0X034698CA,0X95000024, */
/* 0X547B3036,0X05560080,0XFF9AA8B0,0XE7CD5680,0X6BCCD05D,0X0004E0D3,0X564C5B00, */
/* 0XCA0EE6BB,0XE435EB80,0X9CB8FF2B,0XFD5CC36A,0XBF67A60F,0X23000005,0X0B7C5F07, */
/* 0X0900D724,0XFFEC703B,0XBCA9386D,0X35AF3D5F,0X0035030A,0X32E86600,0X475708DA, */
/* 0X2C3EB0A0,0XCBCDFF73,0X70D7CBB6,0XDE530742,0XDD0000B4,0XEA07D78F,0X057E25BF, */
/* 0XFF7FF43D,0X4C5AD76E,0XCD740850,0X008535B0,0X300E4500,0XBAA6A580,0X2030EF07, */
/* 0X2F5EFF0D,0X9BCDE06E,0XF7D936E4,0XBD00008A,0X66AAB553,0XEE66F053,0XFFD35007, */
/* 0X7F3D977A,0X425AE90B,0X003A04B2,0X07BE7C00,0XE90BA4CA,0X00399CB3,0X800FFF36, */
/* 0X0E095AA0,0X569B50D4,0XA60000CA,0X0FF32FFD,0XE4AB9C58,0XFF0B0CAE,0XA7F0BD30, */
/* 0X572087AD,0X009D9C05,0X7B7DCF00,0XBE003A67,0X9F82A8E0,0X7808FF00,0XF82ED42D, */
/* 0X0382F7C8,0X260000A0,0X3D5D0E40,0X770D2A83,0XFF0EA0E7,0X8B488E90,0XD9940C66, */
/* 0X004305C8,0XFFFFFFF0,0XFFFFFFFF,0XFFFFFFFF,0XFFFFF0FF,0XFFFFFFFF,0XFFFFFFFF, */
/* 0XFFF00FFF,0XFFFFFFFF,0XFFFFFFFF,0X0FFFFFFF,0XFFFFFFFF,0XFFFFFFFF,0X0FFFFFFF, */
/* 0X56BDC800,0X495044E4,0X63A5F469,0X99F0FF3D,0X35C3E5A6,0XE5545EC7,0X9B00009B, */
/* 0X0760DD43,0XDFA3E0AE,0XFF7E7889,0X6464CA0B,0X95350255,0X00508690,0XAB0C9000, */
/* 0X8856B627,0XCF33F7EA,0XC08AFF39,0X7CC5F308,0X88C0520B,0XC900002E,0XD87F594E, */
/* 0XF33AE086,0XFFC62FA6,0XEA230085,0XF2DA90E3,0X00656833,0X3760EC00,0X90062808, */
/* 0X8E70837F,0X0ADAFF37,0X40FDA820,0X7D2F487D,0XE00000ED,0X684CB39A,0XD8C02C28, */
/* 0XFF27D2C9,0X37035288,0XAF55230D,0X00000D70,0XB0C05900,0XA0BE070C,0X4D3F3E72, */
/* 0X98BFFF2C,0X40B90092,0X8A889D4A,0XBE000002,0X5E2A33C5,0X89025E8E,0XFFAFF930, */
/* 0X0FA05D5C,0X9DFCD459,0X00F0FA28,0X00707700,0X5C8F2B8F,0XAEAB45B0,0XAF85FF7C, */
/* 0X90C00079,0X5FDD47D2,0XCD000007,0X763760A5,0X8B7EAAF4,0XFFDF086C,0X780C27B9, */
/* 0XB6A4440B,0X006D0CEA,0XD4BD8600,0X288ABFC4,0XAB2F34A9,0X2F0FFFA9,0XB02AADC3, */
/* 0X6E624F36,0XBC0000AC,0XC0700203,0X003CED30,0XFFA25AE9,0X40FF9A59,0X72D2054C, */
/* 0X00A3E05F,0XBD054E00,0X09238C3B,0X6AAC6CEF,0X5730FF2A,0X30ED0297,0X0B6D3300, */
/* 0X00000060,0X6D30AA60,0X26B476AA,0XFF5CE06D,0X0A486636,0X2F0FEA7C,0X007BC26B, */
/* 0X0A6B7200,0X7A6B6C20,0XFF3C5384,0XB305FF0E,0XB4D85D08,0XAECB8040,0X75000078, */
/* 0XF637F335,0X4D22B3D4,0XFFDA06AA,0XF054560C,0X08F06BDE,0X009A5C77,0X4AB87000, */
/* 0X09026E98,0XCAB70ECF,0XC85AFF43,0XC385944B,0XA2408F05,0X3200004C,0XDEA69027, */
/* 0XA002F370,0XFF735A20,0XF59857E2,0XF03AE7EF,0X005BC20F,0X35CD7000,0X5E2700BD, */
/* 0X49B23C84,0X9709FF22,0X0E96D5E2,0XC0D075C0,0X70000030,0X8D75220F,0X2B0B8A42, */
/* 0XFF07E358,0XA5240F00,0X0066FE8C,0X00BC0750,0XE9F02F00,0X25EFCFDB,0X8220622E, */
/* 0X7250FF66,0X8EB03587,0X0F6784CC,0X040000C0,0X8B7F8658,0XCC952ABC,0XFFFCBEE3, */
/* 0XCA0FB783,0X6852ABF0,0X0000439D,0X00000000,0X00000000,0X00000000,0X0000FF00, */
/* 0X00000000,0X00000000,0X00FD0000,0X000F0003,0X001F0003,0X000500FF,0XAAAA0666, */
/* 0XAAAAAAAA,0X0AAAAAAA,0X00000000,0X00000000,0X00000000,0X00000000,0X00000000 */
/* }; */


// -- Globals --

int cobi_fd = -1;

uint8_t default_control_nibbles[COBI_CONTROL_NIBBLES_LEN] = {
    0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA,
    0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA,
    0x0, 0x6, 0x6, 0x6, // pid
    // 0x8, 0x6, 0x6, 0x6, // pid
    0, 0, 0, 5,  // dco_data
    0, 0, 0xF, 0xF, // sample_delay
    0, 0, 1, 0xF,     //  max_fails
    0, 0, 0, 3,    // rosc_time
    0, 0, 0, 0xF,    // shil_time
    0, 0, 0, 3,    // weight_time
    0, 0, 0xF, 0xD    // sample_time
};

// -- End globals --


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

CobiData *cobi_data_mk(size_t num_spins)
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
    d->chip_delay = 0;
    d->descend = false;

    return d;
}

void free_cobi_data(CobiData *d)
{
    _free_array2d(d->programming_bits, d->w);
    free(d->chip1_output);
    free(d->chip1_spins);
    free(d->chip2_output);
    free(d->chip2_spins);
    free(d);
}

void cobi_prepare_weights(
    int **weights, int weight_dim, int shil_val_init, uint8_t *control_bits,
    int **program_array
// , int control_bits_len
) {
    if (weight_dim != 46) {
        printf("Bad weight dimensions: %d by %d\n", weight_dim, weight_dim);
        exit(2);
    }

    int shil_val = hex_mapping(shil_val_init);
    int program_dim = weight_dim + 6; // 52

    // zero outer rows and columns
    for(int k = 0; k < program_dim; k++) {
        program_array[0][k] = 0;
        program_array[1][k] = 0;

        program_array[k][0] = 0;
        program_array[k][1] = 0;

        program_array[k][program_dim-2] = 0;
        program_array[k][program_dim-1] = 0;

        program_array[program_dim-2][k] = 0;
    }

    // add control bits in last row
    for(int k = 0; k < program_dim; k++) {
        program_array[program_dim-1][k] = control_bits[k];
    }

    // add shil
    for (int k = 1; k < program_dim - 1; k++) {
        // rows
        program_array[SHIL_ROW][k]     = shil_val;
        program_array[SHIL_ROW + 1][k] = shil_val;

        // columns are populated in reverse order
        program_array[k][SHIL_ROW - 2]     = shil_val;
        program_array[k][SHIL_ROW - 2 + 1] = shil_val;
    }

    // populate weights
    int row;
    int col;
    for (int i = 0; i < weight_dim; i++) {
        if (i < SHIL_ROW - 2) {
            row = i + 2;
        } else {
            row = i + 4; // account for 2 shil rows and zeroed rows
        }

        for (int j = 0; j < weight_dim; j++) {
            if (j < SHIL_ROW - 2) {
                col = program_dim - 3 - j;
            } else {
                col = program_dim - 5 - j;
            }

            program_array[row][col] = hex_mapping(weights[i][j]);
        }
    }

    // diag
    for (int i = 0; i < program_dim; i++) {
        program_array[program_dim - 1 - i][i] = 0;
    }

    // return program_array;
}

uint32_t *cobi_serialize_programming_bits(int **prog_bits, int prog_dim, int *num_words)
{
    // TODO use more appropriate data type for prog_bits
    // Currently, its assumed that only the least significant 4 bits each element is used

    int num_bits = prog_dim * prog_dim * 4;
    *num_words = num_bits / 32;
    uint32_t *out_words = (uint32_t*)malloc(*num_words * sizeof(uint32_t));

    int word_index = *num_words - 1;
    int nibble_count = 0;
    out_words[word_index] = 0;

    // For now assume prog_bits is an even multiple of 32 so no padding is needed
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

void cobi_wait_while_busy()
{
    uint32_t read_data;
    off_t read_offset;

    // Poll status bit

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

void cobi_read(CobiData *cobi_data)
{
    uint32_t read_data;
    off_t read_offset;

    int num_read_bits = 32;

    cobi_wait_while_busy();

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
            cobi_data->chip1_output[32 * i + j] = ((read_data & mask) >> j) << (num_read_bits - 1 - j);
        }

        // printf("Read data from cobi chip A at offset %ld: 0x%x\n", read_offset, read_data);
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
            cobi_data->chip2_output[32 * i + j] = ((read_data & mask) >> j) << (num_read_bits - 1 - j);
        }

        // printf("Read data from cobi chip B at offset %ld: 0x%x\n", read_offset, read_data);
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

    // printf("Process time: 0x%x\n", read_data);

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
    // Converts raw bits to spins, ie: 0 --> 1; 1 --> -1
    for (int i = 0; i < 46; i++) {
        cobi_data->chip1_spins[i] = cobi_data->chip1_output[i + 28] == 0 ? 1 : -1;
        cobi_data->chip2_spins[i] = cobi_data->chip2_output[i + 28] == 0 ? 1 : -1;
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

void cobi_program_weights(uint32_t *program, int num_words)
{
    pci_write_data write_data;
    int write_count = 0;

    if (num_words != (52 * 52 / 8)) {
        perror("Bad program size\n");
        exit(-9);
    }

    // printf("Programming chip\n");

    cobi_wait_while_busy();

    /* /\* Perform write operations *\/ */
    for (int i = 0; i < RAW_BYTE_COUNT; ++i) {
        write_data.offset = 9 * sizeof(uint32_t);
        write_data.value = program[i];    // data to write
        /* write_data.value = rawData[i]; // use static data */
        write_data.value = ((write_data.value & 0xFFFF0000) >> 16) |
            ((write_data.value & 0x0000FFFF) << 16);
        if (write(cobi_fd, &write_data, sizeof(write_data)) != sizeof(write_data)) {
            perror("Failed to write to device");
            close(cobi_fd);
            exit(-10);
        } else {
            write_count++;
        }
    }

    // printf("Num writes: %d\n", write_count);

    // printf("Programming completed\n");
}

void cobi_reset(int dev_fd)
{
    pci_write_data write_data;
    write_data.offset = 8 * sizeof(uint32_t);
    write_data.value = 0x00000000;    // initialize axi interface control
    // printf("Initialize cobi chips interface controller\n");

    if (write(dev_fd, &write_data, sizeof(write_data)) != sizeof(write_data)) {
        perror("Failed to write to device");
        close(dev_fd);
        exit(2);
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

typedef struct {
    int Q[46][46];
    int energy;
    int spins[46];
    int debug;
} nums;

void solveQ(nums* obj){

    srand(time(NULL));
    const char device_file[] = "/dev/cobi_pcie_card1";
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

    CobiData *cbd = cobi_data_mk(46);
    memset(cbd->chip1_output, 0, sizeof(uint8_t) * CHIP_OUTPUT_SIZE);
    // int **weights = _gen_rand_array2d(46,46);

    int** weights = _malloc_array2d(46, 46);

    int i, j;
    for (i = 0; i < 46; i++) {
        for (j = 0; j < 46; j++) {
            weights[i][j] = obj->Q[i][j];
        }
    }

    cobi_prepare_weights(weights, 46, 7, default_control_nibbles,
                         cbd->programming_bits
                        );
    int num_words; // (num bits / bits per chunk) == num chunks
    uint32_t *data_words = cobi_serialize_programming_bits(cbd->programming_bits, 52, &num_words);

    if(obj->debug){
       printf("Serialization of programming bits:\n");
       for (int i = 0; i < num_words; i++) {
          printf("0X%08X,", data_words[i]);
          if (i % 7  == 6) {
             printf("\n");
          }
       }
       printf("\n");
    }

    // printf("46 by 46 weight matrix:\n");
    if(obj->debug)
        print_array2d(weights, 46, 46);

    double startTime = omp_get_wtime();
    cobi_program_weights(data_words, num_words);
    cobi_read(cbd);
    double finalTime = omp_get_wtime();

    if(obj->debug){
        printf("Wall time: %f\n", finalTime - startTime);
        printf("omp tick: %f\n", omp_get_wtick());
        // printf("Descend: %d\n", cbd->descend);

        printf("\nParsed output:\nChip 1: \n--\n");
        printf("Problem id: %d\n", cbd->chip1_problem_id);

        printf("Energy: %d\n", cbd->chip1_energy);
        print_spins(cbd->chip1_spins);

        printf("Chip 2: \n--\n");
        printf("Problem id: %d\n", cbd->chip2_problem_id);

        printf("Energy: %d\n", cbd->chip2_energy);
        print_spins(cbd->chip2_spins);
    }

    for (int i=0; i<46; i++)
        obj->spins[i] = cbd->chip1_spins[i];
    obj->energy = cbd->chip1_energy;

    free_cobi_data(cbd);
    cobi_reset(cobi_fd);
    close(cobi_fd);
}
