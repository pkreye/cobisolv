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

#include "cobi.h"

#include "extern.h"  // qubo header file: global variable declarations
#include "macros.h"

// #include "extern.h"  // qubo header file: global variable declarations
// #include "macros.h"

// #include <pigpio.h>
#include <lgpio.h>

#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

static int chip;

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


//
// #define WEIGHT_2  2
// #define WEIGHT_3  3
// #define SCANOUT_CLK  4
// #define SAMPLE_CLK  17
// #define ALL_ROW_HI  27
// #define WEIGHT_1  22
// #define WEIGHT_EN  10
// #define COL_ADDR_4  9
// #define ADDR_EN64_CHIP1  11
// #define COL_ADDR_3  5
// #define COL_ADDR_1  6
// #define COL_ADDR_0  13
// #define ROW_ADDR_2  19
// #define ROW_ADDR_3  26
// #define WEIGHT_5  14
// #define SCANOUT_DOUT64_CHIP2  15
// #define SCANOUT_DOUT64_CHIP1  18
// #define WEIGHT_0  23
// #define ROSC_EN  24
// #define COL_ADDR_5  25
// #define ROW_ADDR_5  8
// #define ADDR_EN64_CHIP2  7
// #define WEIGHT_4  1
// #define COL_ADDR_2  12
// #define ROW_ADDR_1  16
// #define ROW_ADDR_0  20
// #define ROW_ADDR_4  21

#define S_DATA_0  20  // #GPIO.Board(38)
#define S_DATA_1  16  // #GPIO.Board(36)
#define S_DATA_2  19  // #GPIO.Board(35)
#define S_DATA_3  26  // #GPIO.Board(37)
#define S_DATA_4  21  // #GPIO.Board(40)
#define S_DATA_5   8  // #GPIO.Board(24)
#define S_DATA_6  11  // #GPIO.Board(23)
#define S_DATA_7   7  // #GPIO.Board(26)
#define S_DATA_8  13  // #GPIO.Board(33)
#define S_DATA_9   6  // #GPIO.Board(31)
#define S_DATA_10 12  // #GPIO.Board(32)
#define S_DATA_11  5  // #GPIO.Board(29)
#define S_DATA_12  9  // #GPIO.Board(21)
#define S_DATA_13 25  // #GPIO.Board(22)
#define S_DATA_14 10  // #GPIO.Board(19)
#define S_DATA_15 24  // #GPIO.Board(18)

#define S_LAST    1  // #GPIO.Board(28)
#define S_VALID  22  // #GPIO.Board(15)
#define S_READY   4  // #RPI input; GPIO.Board(7)
#define M_LAST    3  // #RPI input; GPIO.Board(5)
#define M_VALID   2  // #RPI input; GPIO.Board(3)
#define M_READY  17  // #GPIO.Board(11)
#define M_DATA   14  // #RPI input; GPIO.Board(8)
#define CLK      27  // #GPIO.Board(13)
#define RESETB   23  // #GPIO.Board(16)

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

const int BLANK_GRAPH[64][64] = {
    {0,0,4,3,3,3,3,0,5,7,5,0,4,7,7,3,3,7,2,5,2,7,2,3,7,3,0,7,3,4,5,3,0,0,3,3,3,7,7,3,7,5,5,5,3,2,1,2,4,5,2,4,2,2,4,5,7,3,3,0,3,1,7,0},
    {0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,7},
    {0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,3},
    {0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,3},
    {0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,4},
    {0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,3},
    {0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,5},
    {0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,7},
    {0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,4},
    {0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,4},
    {0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,3},
    {0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,3},
    {0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,2},
    {0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,3},
    {0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,5},
    {0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5},
    {0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2},
    {0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5},
    {0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3},
    {0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5},
    {0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4},
    {0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7},
    {0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3},
    {0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7},
    {0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6},
    {0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3},
    {0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3},
    {0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0},
    {0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0},
    {0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3},
    {0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4},
    {0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,5},
    {0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3},
    {0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7},
    {0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4},
    {0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3},
    {0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7},
    {0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7},
    {0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1},
    {0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4},
    {0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4},
    {0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7},
    {0,3,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4},
    {0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3},
    {0,7,0,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7},
    {0,7,0,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7},
    {0,2,0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3},
    {0,1,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6},
    {0,0,0,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7},
    {0,7,0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,7},
    {0,2,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4},
    {0,0,0,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,6},
    {0,2,0,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3},
    {0,1,0,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2},
    {0,4,0,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3},
    {0,2,0,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,3},
    {0,4,7,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4},
    {0,0,4,4,4,2,4,7,4,7,7,6,3,7,7,4,4,7,4,4,1,7,7,7,7,4,2,7,3,5,4,3,1,1,2,4,7,7,7,3,7,5,4,5,3,7,7,2,5,5,1,3,3,3,3,4,7,5,3,4,3,4,7,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}
};

// Misc utility functions

int usleep(long usecs)
{
   struct timespec rem;
   struct timespec req= {
       (int)(usecs / 1000000),
       (usecs % 1000000) * 1000
   };

   return nanosleep(&req , &rem);
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

double **_malloc_double_array2d(int w, int h)
{
    double** a = (double**)malloc(sizeof(double *) * w);
    if (a == NULL) {
        fprintf(stderr, "Bad malloc %s %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
        exit(1);
    }

    int i, j;
    for (i = 0; i < w; i++) {
        a[i] = (double*)malloc(sizeof(double) * h);
        for (j = 0; j < h; j++) {
            a[i][j] = 0;
        }
    }

    return a;
}

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

void _free_array2d(void **a, int w) {
    int i;
    for (i = 0; i < w; i++) {
        free(a[i]);
    }
    free(a);
}

void binary_splice_rev(int num, int *bin_list)
{
    // given num place lower 6 bits, in reverse order, into bin_list
    int i;
    int shift = 0;
    for (i = 0; i < 6; i++) {
        bin_list[i] = (num >> shift) & 1;
        shift++;
    }
}

/* _outer_prod

   Assumes `result` has dimensions (`len_a`, `len_b`).
 */
void _outer_prod(int *a, size_t len_a, int *b, size_t len_b, int **result)
{
    size_t i, j;

    for (i = 0; i < len_a; i++) {
        for (j = 0; j < len_b; j++) {
            result[i][j] = a[i] * b[j];
        }
    }
}

void _scalar_mult(int *a, size_t len, int scalar)
{
    size_t i;
    for (i = 0; i < len; i++) {
        a[i] = a[i] * scalar;
    }
}

/* _transpose

   Returns newly allocated 2d array containing the result.
 */
int **_transpose(int **a, int w, int h)
{
    int **result = _malloc_array2d(h, w);

    int i, j;
    for (i = 0; i < w; i++) {
        for (j = 0; j < h; j++) {
            result[j][i] = a[i][j];
        }
    }

    return result;
}

/* Element-wise addition of 2 2D arrays. Result stored in first array. */
void _add_array2d(int **a, int **b, int w, int h)
{
    int i, j;
    for (i = 0; i < w; i++) {
        for (j = 0; j < h; j++) {
            a[i][j] += b[i][j];
        }
    }
}

int **_matrix_mult(int **a, int a_w, int a_h, int **b, int b_h)
{
    // b is assumed to have dimensions (a_h, b_h)
    // int b_w = a_h;

    int **result = _malloc_array2d(a_w, b_h);

    int i, j, k;
    for(i = 0; i < a_w; i++) { // each row of a
        for(j = 0; j < b_h; j++) { // each col in b
            result[i][j] = 0;
            for(k = 0; k < a_h; k++) { // each col a
                result[i][j] += a[i][k] * b[k][j];
            }
        }
    }

    return result;
}

// Elementwise multiplication of 2D arrays
int **_array2d_element_mult(int **a, int **b, int w, int h)
{
    int **result = _malloc_array2d(w, h);

    int i, j;
    for (i = 0; i < w; i++) {
        for (j = 0; j < h; j++) {
            result[i][j] = a[i][j] * b[i][j];
        }
    }

    return result;
}

// void cobi_simple_descent(int *spins, int **weights)
// {
//     // Assumption: NUM_GROUPS == len(weights) == len(weights[i]) == len(spins)
//     int size = NUM_GROUPS;

//     int neg_spins[NUM_GROUPS];
//     for (int i = 0; i < size; i++) {
//         // _scalar_mult(neg_spins, size, -1);
//         neg_spins[i] = spins[i] * -1;
//     }

//     int **cross = _malloc_array2d(size, size);
//     // int cross[NUM_GROUPS][NUM_GROUPS]
//     _outer_prod(spins, size, neg_spins, size, cross);

//     // # prod = cross * (weights+weights.transpose())
//     int **wt_transpose = _transpose(weights, size, size);
//     // add weights to wt_transpose, storing result in wt_transpose
//     _add_array2d(wt_transpose, weights, size, size);
//     int **prod = _array2d_element_mult(cross, wt_transpose, size, size);

//     // # diffs = np.sum(prod,0)
//     int diffs[NUM_GROUPS];
//     // Column-wise sum
//     for (int i = 0; i < size; i++) {
//         diffs[i] = 0;
//         for (int j = 0; j < size; j++) {
//             diffs[i] += prod[j][i];
//         }
//     }
//     // TODO fix recursive structure...
//     _free_array2d((void**)cross, size);
//     _free_array2d((void**)wt_transpose, size);
//     _free_array2d((void**)prod, size);

//     for (int i = 0; i < size; i++) {
//         if (diffs[i] < 0) {
//             spins[i] *= -1;

//             return cobi_simple_descent(spins, weights);
//         }
//     }
// }

// ising subproblem solver
#define CHIP_OUTPUT_SIZE 432
CobiData *cobi_data_mk(size_t num_spins, int chip_delay, bool descend)
{
    CobiData *d = (CobiData*)malloc(sizeof(CobiData));
    d->probSize = num_spins;
    d->w = 52;
    d->h = 52;
    d->programming_bits = _malloc_array2d(d->w, d->h);
    d->chip_output = (uint8_t*)malloc(sizeof(uint8_t) * CHIP_OUTPUT_SIZE);

    d->num_samples = 1;
    d->spins = _malloc_array1d(num_spins);

    d->chip_delay = chip_delay;

    d->descend = descend;

    return d;
}

void free_cobi_data(CobiData *d)
{
    _free_array2d((void**)d->programming_bits, d->w);
    free(d->chip_output);
    free(d->spins);
    free(d);
}

int cobi_gpio_setup()
{
    GPIO_CLAIM_OUTPUT(chip, S_DATA_0);
    GPIO_CLAIM_OUTPUT(chip, S_DATA_1);
    GPIO_CLAIM_OUTPUT(chip, S_DATA_2);
    GPIO_CLAIM_OUTPUT(chip, S_DATA_3);
    GPIO_CLAIM_OUTPUT(chip, S_DATA_4);
    GPIO_CLAIM_OUTPUT(chip, S_DATA_5);
    GPIO_CLAIM_OUTPUT(chip, S_DATA_6);
    GPIO_CLAIM_OUTPUT(chip, S_DATA_7);
    GPIO_CLAIM_OUTPUT(chip, S_DATA_8);
    GPIO_CLAIM_OUTPUT(chip, S_DATA_9);
    GPIO_CLAIM_OUTPUT(chip, S_DATA_10);
    GPIO_CLAIM_OUTPUT(chip, S_DATA_11);
    GPIO_CLAIM_OUTPUT(chip, S_DATA_12);
    GPIO_CLAIM_OUTPUT(chip, S_DATA_13);
    GPIO_CLAIM_OUTPUT(chip, S_DATA_14);
    GPIO_CLAIM_OUTPUT(chip, S_DATA_15);
    GPIO_CLAIM_OUTPUT(chip, S_LAST);
    GPIO_CLAIM_OUTPUT(chip, S_VALID);
    GPIO_CLAIM_OUTPUT(chip, M_READY);
    GPIO_CLAIM_OUTPUT(chip, CLK);
    GPIO_CLAIM_OUTPUT(chip, RESETB);

    GPIO_CLAIM_INPUT(chip, S_READY);
    GPIO_CLAIM_INPUT(chip, M_LAST);
    GPIO_CLAIM_INPUT(chip, M_VALID);
    GPIO_CLAIM_INPUT(chip, M_DATA);

    GPIO_WRITE(chip, CLK, GPIO_LOW);

    return 0;
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

#define SHIL_ROW 26

void cobi_prepare_weights(
    int **weights, int weight_dim, int shil_val, uint8_t *control_bits,
    int **program_array
// , int control_bits_len
) {
    if (weight_dim != 46) {
        printf("Bad weight dimensions: %d by %d\n", weight_dim, weight_dim);
        exit(2);
    }

    int program_dim = weight_dim + 6; // 52

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

    // return program_array;
}

void cobi_program_weights(int **program_array)
{
    if (Verbose_ > 0) {
        printf("Programming chip\n");
    }

    GPIO_WRITE(chip, M_READY, GPIO_LOW);
    GPIO_WRITE(chip, S_VALID, GPIO_HIGH);

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

    if (Verbose_ > 0) {
        printf("Programming completed\n");
    }
}

/*
 * cobi_read_spins
 *
 * returns spins via `cobi_data->spins`
 */
void cobi_read_spins(CobiData *cobi_data)
{

    for (int i = 0; i < 46; i++) {
        if (cobi_data->chip_output[i + 4] == 0) {
            cobi_data->spins[i] = 1;
        } else {
            cobi_data->spins[i] = -1;
        }
        // printf("%d: %d\n", i, spins[i]);
    }
}

void cobi_read(uint8_t *output)
{
    GPIO_WRITE(chip, M_READY, GPIO_HIGH);

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

int cobi_read_ham(CobiData *cobi_data)
{
        int tmp = cobi_data->chip_output[65];
        for (int i = 64; i >= 50; i--) {
            tmp = (tmp << 1) + cobi_data->chip_output[i];
        }
        int s = 1 << 15;
        int ham_energy = (tmp & (s - 1)) - (tmp & s);

        return ham_energy;
}

void cobi_modify_array_for_pins(int **initial_array, int  **final_pin_array, int problem_size)
{
    int total_0_rows = 63 - problem_size;

    int y_diag = problem_size; //#set to y-location at upper right corner of problem region

    // #part 1: adjust all values within problem regions of array
    int x, y, integer_pin;
    for (x = total_0_rows; x < 63; x++) {
        for (y = 1; y < problem_size + 1; y++) {
            integer_pin = initial_array[x][y];

            if (integer_pin == -7) {
                final_pin_array[x][y] = 0b001110; // # load value of 14.0 to final_array
            } else if (integer_pin == -6) {
                final_pin_array[x][y] = 0b001100;     // # load value of 12.0 to final_array

            } else if (integer_pin == -5) {
                final_pin_array[x][y] = 0b001010;     // # load value of 10.0 to final_array

            } else if (integer_pin == -4) {
                final_pin_array[x][y] = 0b001000;     // # load value of 8.0 to final_array

            } else if (integer_pin == -3) {
                final_pin_array[x][y] = 0b000110;     // # load value of 6.0 to final_array

            } else if (integer_pin == -2) {
                final_pin_array[x][y] = 0b000100;     // # load value of 4.0 to final_array

            } else if (integer_pin == -1) {
                final_pin_array[x][y] = 0b000010;     // # load value of 2.0 to final_array

            } else if (integer_pin == 0) {
                final_pin_array[x][y] = 0b000000;     // # load value of 0.0 to final_array

            } else if (integer_pin == 1) {
                final_pin_array[x][y] = 0b000011;     // # load value of 3.0 to final_array

            } else if (integer_pin == 2) {
                final_pin_array[x][y] = 0b000101;     // # load value of 5.0 to final_array

            } else if (integer_pin == 3) {
                final_pin_array[x][y] = 0b000111;     // # load value of 7.0 to final_array

            } else if (integer_pin == 4) {
                final_pin_array[x][y] = 0b001001;     // # load value of 9.0 to final_array

            } else if (integer_pin == 5) {
                final_pin_array[x][y] = 0b001011;     // # load value of 11.0 to final_array

            } else if (integer_pin == 6) {
                final_pin_array[x][y] = 0b001101;     // # load value of 13.0 to final_array

            } else if (integer_pin == 8) {
                final_pin_array[x][y] = 0b011111;     // # load the strong positive coupling to the final_array
            } else if (integer_pin == -8) {
                final_pin_array[x][y] = 0b111110;     // # load the strong negative coupling to the final_array
            } else { // # integer_pin == 7
                // TODO rectify value in comment with actual value being assigned
                if (y == y_diag) { // #along diagonal
                    final_pin_array[x][y] = 0b001111; // # load value of 31.0 to final_array
                } else {
                    final_pin_array[x][y] = 0b001111; // # load value of 15.0 to final_array

                }
            }
        }
        y_diag--;
    }

    // #part 2 - adjust remaining 7s in diagonal
    y_diag = problem_size;
    for (x = 0; x < 64; x++) {
        for (y = 0; y < 64; y++) {
            integer_pin = initial_array[x][y];
            if (y == y_diag && integer_pin == 7) {
                final_pin_array[x][y] = 0b001111;
            }
        }
    }
    /* return final_pin_array; */
}

int *cobi_test_multi_times(
    CobiData *cobi_data, int sample_times, int size, int8_t *solution
) {
    int times = 0;
    int *all_results = _malloc_array1d(sample_times);
    int cur_best = 0;
    int res;

    while (times < sample_times) {
        cobi_reset();

        while(GPIO_READ(chip, S_READY) != 1) {
            GPIO_WRITE(chip, CLK, GPIO_HIGH);
            GPIO_WRITE(chip, CLK, GPIO_LOW);
        }

        cobi_program_weights(cobi_data->programming_bits);

        memset(cobi_data->chip_output, 0, sizeof(uint8_t) * CHIP_OUTPUT_SIZE);

        while(GPIO_READ(chip, M_VALID) != 1) {
            GPIO_WRITE(chip, CLK, GPIO_HIGH);
            GPIO_WRITE(chip, CLK, GPIO_LOW);
        }

        cobi_read(cobi_data->chip_output);


        if (Verbose_ > 2) {
            printf("\nSample number %d\n", times);
        }

        cobi_read_spins(cobi_data);

        res = cobi_read_ham(cobi_data);  //# calculate H energy from chip data

        all_results[times] = res;

        if (res < cur_best) {
            cur_best = res;
            for (size_t i = 0; i < cobi_data->probSize; i++ ) {
                solution[i] = cobi_data->spins[i];
            }
        }

        times += 1;

        if (Verbose_ > 2) {
            printf(", best = %d ",  cur_best);
        }
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

    for (i = 0; i < size; i++) {
        for (j = i; j < size; j++) {
            cur_v = ising[i][j];
            if (cur_v == 0) {
                norm[i][j] = 0;
                norm[j][i] = 0;
            } else if (i == j) {
                double scaled = (28 * (cur_v - min)/ (max - min)) - 14;
                  norm[i][i] = scaled / 2;
            } else {
                double scaled = ((28 * (cur_v - min)) / (max - min)) - 14;
                int symmetric_val = (int) round(scaled / 2);
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

int cobi_init()
{
    chip = lgGpiochipOpen(4); // "/dev/gpiochip4"
    if(chip < 0)
    {
        printf("ERROR: lgGpiochipOpen failed: %d\n", chip);
        exit(1);
    }

    // setup GPIO pins
    cobi_gpio_setup();

    if(Verbose_ > 0) printf("GPIO initialized successfully\n");

    return chip;
}

bool cobi_established()
{
    // TODO How to verify existence of cobi chip?
    // connection = getenv("DW_INTERNAL__CONNECTION");
    // if (connection == NULL) {
    return true;
    // }
    // return true;
}

void cobi_solver(
    double **qubo, int numSpins, int8_t *qubo_solution, int num_samples, int chip_delay, bool descend
) {
    if (numSpins > 46) {
        printf("Quitting.. cobi_solver called with size %d. Cannot be greater than 46.\n", numSpins);
        exit(2);
    }

    CobiData *cobi_data = cobi_data_mk(numSpins, chip_delay, descend);
    int8_t *ising_solution = (int8_t*)malloc(sizeof(int8_t) * numSpins);
    double **ising = _malloc_double_array2d(numSpins, numSpins);
    int **norm_ising = _malloc_array2d(numSpins, numSpins);

    ising_from_qubo(ising, qubo, numSpins);
    cobi_norm_val(norm_ising, ising, numSpins);

    cobi_prepare_weights(norm_ising, 46, 7, default_control_bytes, cobi_data->programming_bits);

    // Convert solution from QUBO to ising
    ising_solution_from_qubo_solution(ising_solution, qubo_solution, numSpins);

    //
    int *results = cobi_test_multi_times(
        cobi_data, num_samples, numSpins, ising_solution
    );

    // Convert ising solution back to QUBO form
    qubo_solution_from_ising_solution(qubo_solution, ising_solution, numSpins);

    free(results);
    free_cobi_data(cobi_data);
    free(ising_solution);
    _free_array2d((void**)ising, numSpins);
    _free_array2d((void**)norm_ising, numSpins);
}

void cobi_close()
{
    if (Verbose_ > 0) {
        printf("lgpio clean up\n");
    }

    lgGpiochipClose(chip);
}

#ifdef __cplusplus
}
#endif
