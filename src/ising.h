#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <pigpio.h>

int usleep(long usecs);

#define GPIO_WRITE(pin, val)                                            \
    if (gpioWrite(pin, val) != 0) {                                     \
        printf("\n ERROR: bad gpio write: %s(%s.%d)\n\n", __FUNCTION__, __FILE__, __LINE__); \
        exit(9);                                                        \
    }


#define GPIO_WRITE_DELAY(pin, val, delay)                               \
    if (gpioWrite(pin, val) != 0) {                                     \
        printf("\n ERROR: bad gpio write: %s(%s.%d)\n\n", __FUNCTION__, __FILE__, __LINE__); \
        exit(9);                                                        \
    }                                                                   \
    usleep(delay);


typedef struct IsingData {
    size_t probSize;
    size_t w;
    size_t h;
    int **programming_bits;
    uint8_t *chip2_test;
    int num_samples;
    int *spins;
    int64_t chip_delay;
    bool descend;

    int sample_test_count;
    int *prev_spins;
    /* int **graph_arr; */
    /* uint8_t *samples; */
} IsingData;

/* void ising_from_qubo(double **qubo, size_t, double **ising); */

bool ising_established(void);

int ising_init(void);

void ising_close(void);

void ising_solver(double **, int, int8_t *, int, int, bool);

#ifdef __cplusplus
}
#endif
