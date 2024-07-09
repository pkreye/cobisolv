
#include <stdio.h>
#include <lgpio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

// --
int usleep(long usecs);

int default_weights[46][46] = {
{0,0,0,0,0,0,-1,0,0,0,-3,0,0,0,0,0,0,0,0,0,0,-3,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,-1,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,-2,0,0,0,-1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-2,0},
{0,0,0,0,0,0,0,0,-1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,-1,0,0,0,0,0,0,0,0,0,0},
{0,1,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-4,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,-2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,-1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-4,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,-4,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,-2,0,0,0,0,0,0},
{-2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,-1,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,-3,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
{0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,-3,-2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,-2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,2,0,0,0,0,0,0,-3,0,0,0,-1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-1,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-2,0,0,0,0,0,0,3,0,0,0,0,0,0,0,-4},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-4,0,0,0},
{-2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0},
{0,0,0,0,3,0,0,0,0,0,-1,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,-1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,-3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,2,0,0,0,0,0,0,0,0,0,0,-3,0,0,0,0,-1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{1,3,0,-1,0,0,0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,-3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,-2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-3,0},
{0,0,0,0,0,0,0,0,0,0,-2,0,0,0,0,0,0,0,0,0,-3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
{0,0,-1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,-3,0,0,0,0},
{0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0,0,-3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
};

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

uint8_t **_malloc_array2d_uint8(int w, int h)
{
    uint8_t** a = (uint8_t**)malloc(sizeof(uint8_t *) * w);
    if (a == NULL) {
        fprintf(stderr, "Bad malloc %s %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
        exit(1);
    }

    int i, j;
    for (i = 0; i < w; i++) {
        a[i] = (uint8_t*)malloc(sizeof(uint8_t) * h);
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


int _rand_int_normalized()
{
// generate random int normalized to -14 to 14
    if (rand() % 2 == 1) {
        return rand() % 8;
    } else {
        return -1 * (rand() % 8);
    }
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
// --

#define GPIO_READ(pi, pin) lgGpioRead(pi, pin)

#define GPIO_WRITE(pi, pin, val)                                         \
    if (lgGpioWrite(pi, pin, val) != 0) {                                  \
        printf("\n ERROR: bad gpio write on %d: %s(%d)\n\n", pin, __FUNCTION__, __LINE__); \
        exit(9);                                                        \
    }

#define GPIO_WRITE_DELAY(pi, pin, val, delay)                            \
    if (lgGpioWrite(pi, pin, val) != 0) {                               \
        printf("\n ERROR: bad gpio write on %d: %s(%d)\n\n", pin,  __FUNCTION__, __LINE__); \
        exit(9);                                                        \
    }
    usleep(delay);

#define GPIO_OUTPUT_FLAGS 0
#define GPIO_CLAIM_OUTPUT(pi, pin)                                      \
    if (lgGpioClaimOutput(pi, GPIO_OUTPUT_FLAGS, pin, 0) != 0) {        \
        printf("\nERROR: GPIO cannot set output mode on %d: %s(%d)\n\n", pin, __FUNCTION__, __LINE__); \
        exit(9);                                                        \
    }

#define GPIO_INPUT_FLAGS 0
#define GPIO_CLAIM_INPUT(pi, pin)                                       \
    if (lgGpioClaimInput(pi, GPIO_INPUT_FLAGS, pin) != 0) {             \
        printf("\nERROR: GPIO cannot set input mode on %d: %s(%d)\n\n", pin, __FUNCTION__, __LINE__); \
        exit(9);                                                        \
    }

#define GPIO_HIGH 1
#define GPIO_LOW  0

// --
// --

int chip;

// #--------------------PIN INITIALIZATION--------------------#

#define S_DATA_0    20  // #GPIO.Board(38)
#define S_DATA_1    16  // #GPIO.Board(36)
#define S_DATA_2    19  // #GPIO.Board(35)
#define S_DATA_3    26  // #GPIO.Board(37)
#define S_DATA_4    21  // #GPIO.Board(40)
#define S_DATA_5    8   // #GPIO.Board(24)
#define S_DATA_6    11  // #GPIO.Board(23)
#define S_DATA_7    7   // #GPIO.Board(26)
#define S_DATA_8    13  // #GPIO.Board(33)
#define S_DATA_9    6   // #GPIO.Board(31)
#define S_DATA_10   12  // #GPIO.Board(32)
#define S_DATA_11   5   // #GPIO.Board(29)
#define S_DATA_12   9   // #GPIO.Board(21)
#define S_DATA_13   25  // #GPIO.Board(22)
#define S_DATA_14   10  // #GPIO.Board(19)
#define S_DATA_15   24  // #GPIO.Board(18)
#define S_LAST    1     // #GPIO.Board(28)
#define S_VALID   22    // #GPIO.Board(15)
#define S_READY   4     // #RPI input; GPIO.Board(7)
#define M_LAST    3     // #RPI input; GPIO.Board(5)
#define M_VALID    2    // #RPI input; GPIO.Board(3)
#define M_READY   17    // #GPIO.Board(11)
#define M_DATA    14    // #RPI input; GPIO.Board(8)
#define CLK       27    // #GPIO.Board(13)
#define RESETB    23    // #GPIO.Board(16)

#define S_DATA_LEN 16
int S_DATA[S_DATA_LEN] = {
    S_DATA_0 ,
    S_DATA_1 ,
    S_DATA_2 ,
    S_DATA_3 ,
    S_DATA_4 ,
    S_DATA_5 ,
    S_DATA_6 ,
    S_DATA_7 ,
    S_DATA_8 ,
    S_DATA_9 ,
    S_DATA_10,
    S_DATA_11,
    S_DATA_12,
    S_DATA_13,
    S_DATA_14,
    S_DATA_15
};

#define OUTPUT_PIN_COUNT 21
int OUTPUT_PINS[OUTPUT_PIN_COUNT] = {
    S_DATA_0,
    S_DATA_1,
    S_DATA_2,
    S_DATA_3,
    S_DATA_4,
    S_DATA_5,
    S_DATA_6,
    S_DATA_7,
    S_DATA_8,
    S_DATA_9,
    S_DATA_10,
    S_DATA_11,
    S_DATA_12,
    S_DATA_13,
    S_DATA_14,
    S_DATA_15,
    S_LAST,
    S_VALID,
    M_READY,
    CLK,
    RESETB
};

#define INPUT_PIN_COUNT 4
int INPUT_PINS[INPUT_PIN_COUNT] = {
    S_READY,
    M_LAST,
    M_VALID,
    M_DATA
};

int usleep(long usecs)
{
   struct timespec rem;
   struct timespec req= {
       (int)(usecs / 1000000),
       (usecs % 1000000) * 1000
   };

   return nanosleep(&req , &rem);
}


void cobi_reset()
{
    // #reset startup
    GPIO_WRITE(chip, RESETB, GPIO_HIGH);
    GPIO_WRITE(chip, M_READY, GPIO_LOW);

    for (int i = 0; i < 16; i ++) {
        GPIO_WRITE(chip, S_DATA[i], GPIO_LOW);
    }

    GPIO_WRITE(chip, S_LAST, GPIO_LOW);

    // #reset
    GPIO_WRITE(chip, CLK, GPIO_HIGH);
    GPIO_WRITE(chip, RESETB, GPIO_LOW);
    GPIO_WRITE(chip, CLK, GPIO_LOW);
    usleep(1000);

    GPIO_WRITE(chip, CLK, GPIO_HIGH);
    usleep(1000);
    GPIO_WRITE(chip, RESETB, GPIO_HIGH);
    GPIO_WRITE(chip, CLK, GPIO_LOW);

    usleep(1000);
    GPIO_WRITE(chip, CLK, GPIO_HIGH);
    usleep(100);
    GPIO_WRITE(chip, CLK, GPIO_LOW);
    usleep(100);
    GPIO_WRITE(chip, CLK, GPIO_HIGH);
    usleep(100);
    GPIO_WRITE(chip, CLK, GPIO_LOW);
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

#define COBI_CONTROL_BYTES_LEN 52
uint8_t default_control_bytes[COBI_CONTROL_BYTES_LEN] = {
    0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA,
    0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA, 0xA,
    0x4, 0xF, 0xF, 0xF, // pid
    0, 0, 0, 5,  // dco_data
    0, 0, 0xF, 0xF, // sample_delay
    0, 0, 1, 0xF,     //  max_fails
    0, 0, 0, 3,    // rosc_time
    0, 0, 0, 0xF,    // shil_time
    0, 0, 0, 3,    // weight_time
    0, 0, 0xF, 0xD    // sample_time
};
// AAAAAAAAAAAAAAAAAAAA4FFF000500FF001F0003000F000300FD

/*
def mk_data_str(
pid=0x4FFF,
dco_data=0x005,
sample_delay=0x00FF,
max_fails=31,
rosc_time=0x3,
shil_time=0xF,
weight_time=0x3,
sample_time=0xFD
):
return data_fmt.format(pid,dco_data,sample_delay,max_fails,rosc_time,shil_time,weight_time,sample_time) */

/* uint8_t *cobi_mk_control_bytes( */
/*     int pid, */
/*     int dco_data, */
/*     int sample_delay, */
/*     int max_fails, */
/*     int rosc_time, */
/*     int shil_time, */
/*     int weight_time, */
/*     int sample_time */
/* ) { */
/*     uint8_t *bytes = malloc(sizeof(uint8_t) * COBI_CONTROL_BYTES_LEN); */
/*     int i = 0; */
/*     for (i = 0; i < 21; i++) { */
/*         bytes[i] = 0xA; */
/*     } */

/*     bytes[i++] */

/* } */


void _print_control_bits(uint8_t *bits) {
    printf("--control bits--\n");
    for (int i = 0; i < COBI_CONTROL_BYTES_LEN; i++) {
        printf("%d: %#06x\n", i, bits[i]);
    }
    printf("----\n");
}

void _print_array2d(double **a, int w, int h)
{
    int x, y;
    for (x = 0; x < w; x++) {
        printf("%02d: ", x);
        for (y = 0; y < h; y++) {
            printf("%lf ", a[x][y]);
        }
        printf("\n");
    }
}

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

void _print_array2d_uint8(uint8_t **a, int w, int h)
{
    int x, y;
    for (x = 0; x < w; x++) {
        printf("%02d: ", x);
        for (y = 0; y < h; y++) {
            printf("%02x ", a[x][y]);
        }
        printf("\n");
    }
}


#define SHIL_ROW 26

uint8_t **cobi_prepare_weights(
    int **weights, int weight_dim, int shil_val, uint8_t *control_bits // , int control_bits_len
) {
    if (weight_dim != 46) {
        printf("Bad weight dimensions: %d by %d\n", weight_dim, weight_dim);
        exit(2);
    }

    int program_dim = weight_dim + 6; // 52

    uint8_t **program_array = _malloc_array2d_uint8(program_dim, program_dim); // 52 by 52

    // zero first, add control bits
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

    return program_array;
}

void cobi_read(int *output)
{
    GPIO_WRITE_DELAY(chip, M_READY, GPIO_HIGH, 100);

    GPIO_WRITE(chip, CLK, GPIO_HIGH);
    GPIO_WRITE(chip, CLK, GPIO_LOW);

    int bit_count = 0;
    output[bit_count] = GPIO_READ(chip, M_DATA);
    int m_last = GPIO_READ(chip, M_LAST);
    int m_valid = GPIO_READ(chip, M_VALID);
    /* printf("m_last: %d, m_valid: %d ", m_last, m_valid); */
    while(m_last == GPIO_LOW && m_valid == GPIO_HIGH) {
        bit_count++;
        GPIO_WRITE(chip, CLK, GPIO_HIGH);
        output[bit_count] = GPIO_READ(chip, M_DATA);
        GPIO_WRITE(chip, CLK, GPIO_LOW);

        if (bit_count > 431) {
            printf("Data Read Error: %d\n", bit_count);
            exit(2);
        }

        m_last = GPIO_READ(chip, M_LAST);
        m_valid = GPIO_READ(chip, M_VALID);
        /* printf("m_last: %d, m_valid: %d ", m_last, m_valid); */
    }

    GPIO_WRITE(chip, CLK, GPIO_HIGH);
    GPIO_WRITE(chip, CLK, GPIO_LOW);
}

void cobi_write(uint8_t **program_array)
{
    GPIO_WRITE(chip, M_READY, GPIO_LOW);
    GPIO_WRITE(chip, S_VALID, GPIO_HIGH);

    /* printf("Write: m_ready: %d; s_valid: %d\n", GPIO_READ(chip, M_READY), GPIO_READ(chip, S_VALID)); */

    for (int i = 0; i < 52; i++) {
        for (int j = 12; j >= 0; j--) {

            uint8_t bits[16];

            for (int k = 0; k < 4; k++) {
                int v = program_array[i][j * 4 + k]; // current 4 bits

                int bit_index = 12 - 4*k;

                bits[bit_index + 3] = (v & 0x8) >> 3;
                bits[bit_index + 2] = (v & 0x4) >> 2;
                bits[bit_index + 1] = (v & 0x2) >> 1;
                bits[bit_index]     = v &  0x1;
            }

            for (int k = 0; k < 16; k++) {
                GPIO_WRITE(chip, S_DATA[k], bits[k]);
            }

            // # once all bits are assigned to s_data_#,
            // # indicate this is the last data set before clk's rising edge
            if (i == 51 && j == 0) {
                GPIO_WRITE(chip, S_LAST, GPIO_HIGH);
            }

            GPIO_WRITE(chip, CLK, GPIO_HIGH);
            GPIO_WRITE(chip, CLK, GPIO_LOW);
        }
    }

    GPIO_WRITE(chip, S_LAST, GPIO_LOW);
    GPIO_WRITE(chip, S_VALID, GPIO_LOW); // #s_valid low for 2 clk cycles

    GPIO_WRITE_DELAY(chip, CLK, GPIO_HIGH, 100);
    GPIO_WRITE_DELAY(chip, CLK, GPIO_LOW, 100);
    GPIO_WRITE_DELAY(chip, CLK, GPIO_HIGH, 100);
    GPIO_WRITE(chip, CLK, GPIO_LOW);

    GPIO_WRITE(chip, S_VALID, GPIO_LOW);

    /* printf("Write End: m_ready: %d; s_valid: %d\n", GPIO_READ(chip, M_READY), GPIO_READ(chip, S_VALID)); */
}

void cleanup()
{
    lgGpiochipClose(chip);
}

/* int *cobi_run_problem() */


#define LFLAGS 0

int main()
{
    srand(time(NULL));

    if (atexit(cleanup) != 0) {
        fprintf(stderr, "Failed to register exit function\n");
        exit(1);
    }

    chip = lgGpiochipOpen(4); //"/dev/gpiochip4"
    if(chip < 0)
    {
        printf("ERROR: lgGpiochipOpen failed: %d\n", chip);
        exit(1);
    }
    int res = -9;

    for (int i = 0; i < OUTPUT_PIN_COUNT; i++) {
        res = lgGpioClaimOutput(chip, LFLAGS, OUTPUT_PINS[i], 0);
        if (res != LG_OKAY) {
            printf("could not claim output pin: %d %d\n", i, res);
            exit(1);
        }
    }

    for (int i = 0; i < INPUT_PIN_COUNT; i++) {
        res = lgGpioClaimInput(chip, LFLAGS, INPUT_PINS[i]);
        if (res != LG_OKAY) {
            printf("could not claim input pin: %d %d\n", i, res);
            exit(1);
        }
    }

    /* int cur_val = 0; */
    /* for (int i = 0; i < 10; i++) { */
    /*     res = lgGpioRead(chip, CLK); */
    /*     printf("clk: %d\n", res); */

    /*     /\* res = lgGpioRead(chip, S_READY); *\/ */
    /*     /\* printf("s_ready: %d\n", res); *\/ */
    /*     cur_val = cur_val ^ 1; */
    /*     lgGpioWrite(chip, CLK, cur_val); */

    /*     usleep(100); */
    /* } */

    /* int **weights = _gen_rand_array2d(48,48); */
    int **weights = _malloc_array2d(46,46);
    for(int i = 0; i < 46; i++) {
        for(int j = 0; j < 46; j++) {
            weights[i][j] = default_weights[i][j];
        }
    }
    uint8_t **program_array = cobi_prepare_weights(weights, 46, 7, default_control_bytes);

    #define OUTPUT_SIZE 432
    int output[OUTPUT_SIZE];
    #define RUN_COUNT 3


    for (int count = 0; count < RUN_COUNT; count++) {

        cobi_reset();


        printf("\n--\nRun count %d\n--\n", count);

        while(GPIO_READ(chip, S_READY) != 1) {
            GPIO_WRITE_DELAY(chip, CLK, GPIO_HIGH, 10);
            GPIO_WRITE_DELAY(chip, CLK, GPIO_LOW, 10);
        }

        cobi_write(program_array);

        memset(output, 0, sizeof(int) * OUTPUT_SIZE);

        while(GPIO_READ(chip, M_VALID) != 1) {
            GPIO_WRITE_DELAY(chip, CLK, GPIO_HIGH, 10);
            GPIO_WRITE_DELAY(chip, CLK, GPIO_LOW, 10);
        }

        cobi_read(output);

        printf("--output--\n");
        printf("--spins:\n");
        int spins[46];
        int manual_energy = 0;
        for (int i = 0; i < 46; i++) {
            if (output[i + 4] == 0) {
                spins[i] = 1;
            } else {
                spins[i] = -1;
            }

            printf("%d: %d\n", i, spins[i]);
        }
        printf("--end spins--");

        for (int i = 0; i < 46; i++) {
            for (int j = i; j < 46; j++) {
                if (i == j && weights[i][i] != 0) {
                    manual_energy += weights[i][i] * spins[i];
                } else if (weights[i][j] != 0) {
                    manual_energy += (weights[j][i] + weights[i][j]) * spins[i] * spins[j];
                }
            }
        }

        printf("--manual energy calc: %d\n", manual_energy);

        int tmp = output[65];
        for (int i = 64; i >= 50; i--) {
            tmp = (tmp << 1) + output[i];
        }
        int s = 1 << 15;
        int ham_energy = (tmp & (s - 1)) - (tmp & s);
        printf("--ham energy: %d", ham_energy);
        printf("--end output--\n\n");
    }
}
