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
// #include <pigpio.h>
#include <lgpio.h>

int usleep(long usecs);

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
    }                                                                   \
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

typedef struct CobiData {
    size_t probSize;
    size_t w;
    size_t h;
    int **programming_bits;
    uint8_t *chip_output;
    int num_samples;
    int *spins;
    int64_t chip_delay;
    bool descend;

    int sample_test_count;
    int *prev_spins;
    /* int **graph_arr; */
    /* uint8_t *samples; */
} CobiData;

/* void cobi_from_qubo(double **qubo, size_t, double **cobi); */

bool cobi_established(void);

int cobi_init(void);

void cobi_close(void);

void cobi_solver(double **, int, int8_t *, int, int, bool);

#ifdef __cplusplus
}
#endif
