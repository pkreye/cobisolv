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

int usleep(long usecs);

typedef struct CobiData {
    size_t probSize;
    size_t w;
    size_t h;
    int **programming_bits;

    uint32_t *chip1_output;
    uint32_t *chip2_output;

    int *chip1_spins;
    int *chip2_spins;

    uint32_t process_time;

    int num_samples;
    int64_t chip_delay;
    bool descend;

    int sample_test_count;
} CobiData;

bool cobi_established(void);
int cobi_init(void);
void cobi_close(void);
void cobi_solver(double **, int, int8_t *, int, int, bool);

#ifdef __cplusplus
}
#endif
