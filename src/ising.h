#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

typedef struct IsingData {
    size_t probSize;
    size_t w;
    size_t h;
    int **programming_bits;
    uint8_t *chip2_test;
    int num_samples;
    int **spins;
    /* int **graph_arr; */
    /* uint8_t *samples; */
} IsingData;

bool ising_established(void);

int ising_init(void);

void ising_close(void);

void ising_solver(double **, int, int8_t *);

#ifdef __cplusplus
}
#endif
