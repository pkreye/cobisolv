// #include "extern.h"  // qubo header file: global variable declarations
// #include "macros.h"

#include "ising.h"

// #include "ising_graph_helper.h"

#include <pigpio.h>

#include <stdlib.h>

#ifdef __cplusplus
// extern "C" {
#endif
#define WL_DELAY         0.00001
#define SCAN_CLK_DELAY   0.000001
#define NUM_OF_NODES     100
#define BITS_PER_SAMPLE  8

#define WEIGHT_2  2
#define WEIGHT_3  3
#define SCANOUT_CLK  4
#define SAMPLE_CLK  17
#define ALL_ROW_HI  27
#define WEIGHT_1  22
#define WEIGHT_EN  10
#define COL_ADDR_4  9
#define ADDR_EN64_CHIP1  11
#define COL_ADDR_3  5
#define COL_ADDR_1  6
#define COL_ADDR_0  13
#define ROW_ADDR_2  19
#define ROW_ADDR_3  26
#define WEIGHT_5  14
#define SCANOUT_DOUT64_CHIP2  15
#define SCANOUT_DOUT64_CHIP1  18
#define WEIGHT_0  23
#define ROSC_EN  24
#define COL_ADDR_5  25
#define ROW_ADDR_5  8
#define ADDR_EN64_CHIP2  7
#define WEIGHT_4  1
#define COL_ADDR_2  12
#define ROW_ADDR_1  16
#define ROW_ADDR_0  20
#define ROW_ADDR_4  21

const int ROW_ADDR[6] = {
    ROW_ADDR_0, ROW_ADDR_1,
    ROW_ADDR_2, ROW_ADDR_3,
    ROW_ADDR_4, ROW_ADDR_5
};

const int COL_ADDR[6] = {
    COL_ADDR_0, COL_ADDR_1,
    COL_ADDR_2, COL_ADDR_3,
    COL_ADDR_4, COL_ADDR_5
};

const int WEIGHT[6] = {
    WEIGHT_0, WEIGHT_1,
    WEIGHT_2, WEIGHT_3,
    WEIGHT_4, WEIGHT_5
};


// Misc utility functions


int _rand_int_normalized()
{
// generate random int normalized to -14 to 14
    if (rand() % 2 == 1) {
        return rand() % 15;
    } else {
        return -1 * (rand() % 15);
    }
}

int **_gen_rand_array2d(int w, int h)
{
    int** a = malloc(sizeof(int *) * w);

    int i, j;
    for (i = 0; i < w; i++) {
        a[i] = malloc(sizeof(int) * h);

        for (j = 0; j < h; j++) {
            a[i][j] = _rand_int_normalized();
        }
    }

    return a;
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

// ising subproblem solver

int ising_gpio_setup()
{
        gpioSetMode(WEIGHT_2,    PI_OUTPUT);
        gpioSetMode(WEIGHT_3,    PI_OUTPUT);
        gpioSetMode(SCANOUT_CLK, PI_OUTPUT);
        gpioSetMode(SAMPLE_CLK,  PI_OUTPUT);
        gpioSetMode(ALL_ROW_HI,  PI_OUTPUT);
        gpioSetMode(WEIGHT_1,    PI_OUTPUT);
        gpioSetMode(WEIGHT_EN,   PI_OUTPUT);
        gpioSetMode(COL_ADDR_4,  PI_OUTPUT);
        gpioSetMode(ADDR_EN64_CHIP1, PI_OUTPUT);
        gpioSetMode(COL_ADDR_3,  PI_OUTPUT);
        gpioSetMode(COL_ADDR_1,  PI_OUTPUT);
        gpioSetMode(COL_ADDR_0,  PI_OUTPUT);
        gpioSetMode(ROW_ADDR_2,  PI_OUTPUT);
        gpioSetMode(ROW_ADDR_3,  PI_OUTPUT);
        gpioSetMode(WEIGHT_5,    PI_OUTPUT);
        gpioSetMode(SCANOUT_DOUT64_CHIP2, PI_INPUT);
        gpioSetMode(SCANOUT_DOUT64_CHIP1, PI_INPUT);
        gpioSetMode(WEIGHT_0,    PI_OUTPUT);
        gpioSetMode(ROSC_EN,     PI_OUTPUT);
        gpioSetMode(COL_ADDR_5,  PI_OUTPUT);
        gpioSetMode(ROW_ADDR_5,  PI_OUTPUT);
        gpioSetMode(ADDR_EN64_CHIP2, PI_OUTPUT);
        gpioSetMode(WEIGHT_4,    PI_OUTPUT);
        gpioSetMode(COL_ADDR_2,  PI_OUTPUT);
        gpioSetMode(ROW_ADDR_1,  PI_OUTPUT);
        gpioSetMode(ROW_ADDR_0,  PI_OUTPUT);
        gpioSetMode(ROW_ADDR_4,  PI_OUTPUT);

        gpioWrite(SAMPLE_CLK, PI_LOW);
}

void ising_weight_pins_low()
{
        gpioWrite(WEIGHT_0, PI_LOW);
        gpioWrite(WEIGHT_1, PI_LOW);
        gpioWrite(WEIGHT_2, PI_LOW);
        gpioWrite(WEIGHT_3, PI_LOW);
        gpioWrite(WEIGHT_4, PI_LOW);
        gpioWrite(WEIGHT_5, PI_LOW);
}

void ising_set_addr(int *addrs, int *bin_num_list)
{
    int addr_name;
    int i;
    for (i = 0; i < 6; i++) {
        addr_name = addrs[i];
        if (bin_num_list == 1) {
            gpioWrite(addr_name, PI_HIGH);
        } else {
            gpioWrite(addr_name, PI_LOW);
        }
    }
}

void ising_program_weights(int **programming_bits)
{
    if (Verbose_ > 0) {
        printf("Programming chip\n");
    }

    int enable_pin_name = ADDR_EN64_CHIP2;

    // initialize binary lists
    int bin_row_list[6];
    int bin_col_list[6];
    int bin_weight_list[6];

    // reset pins for programming
    ising_weight_pins_low();
    gpioWrite(ALL_ROW_HI, PI_LOW);

    // run through each row of 64x64 cells in COBI/COBIFREEZE
    int x = 0;
    int y = 0;
    for (x = 0; x < 64; x++) { // #run through each row of 64x64 cells in COBI/COBIFREEZE
        binary_splice_rev(x, bin_row_list);
        for (y = 0; y < 64; y++) { // #run through each cell in a given row
            binary_splice_rev(y, bin_col_list);
            binary_splice_rev(programming_bits[x][y], bin_weight_list);

            ising_set_addr(ROW_ADDR, bin_row_list); // #assign the row number
            ising_set_addr(COL_ADDR, bin_col_list); // #assign the column number

            gpioWrite(enable_pin_name, PI_HIGH);

            // #set weight of 1 cell
            ising_set_addr(WEIGHT, bin_weight_list); // #assign the weight corresponding to current cell
            // # time.sleep(.001)  # Delay removed since COBIFIXED65 board does not have any level shifters which causes additional signal delay
            gpioWrite(enable_pin_name, PI_LOW);
            ising_weight_pins_low(); // #reset for next address
        }
    }

    if (Verbose_ > 0) {
        printf("Programming completed\n");
    }
}

double ising_cal_energy_ham() //# cal_energy_ham(self, size, graph_input_file)
{
    // TODO
}

double ising_cal_energy()
// #cal_energy(self, sample_bits, size, sample_index, ising_data_array, sample_times, graph_problem_file,verbose=True, return_spins=False,store_time=[]):
{
    int i = 0;

    // chip_file_name="run_files/chip2_test.txt";
    // all_samples_chip2 = np.zeros((504), dtype=np.int8);
    int all_samples_chip2[504];

    gpioWrite(ROSC_EN, PI_LOW);
    gpioWrite(ROSC_EN, PI_HIGH);

    // TODO Do we need an arbitrary delay here?
    //py:for i in range (2500):
    //py:    pass

    // #print("--- %s seconds ---" % (time.time() - start_time))
    // #store_time.append(time.time() - start_time)
    // #print("--- %s seconds ---" % (time.time() - self.start_time))
    gpioWrite(SAMPLE_CLK, PI_HIGH);
    // #time.sleep(0.0001)
    gpioWrite(SAMPLE_CLK, PI_LOW);

    int bit;
    for (bit = 0; bit < 441; bit++) {
        if (gpioRead(SCANOUT_DOUT64_CHIP2 == 1)) {
            all_samples_chip2[bit] = 1;
        } else {
            all_samples_chip2[bit] = 0;
        }

        if (bit == 440) break;

        gpioWrite(SCANOUT_CLK, PI_HIGH);
        gpioWrite(SCANOUT_CLK, PI_LOW);
    }

    // with open(chip_file_name, 'w') as f2:
    //     count_node_rows = 0
    //     for bit in range(441):
    //         if (bit % 7) == 0:
    //             f2.write('node ')
    //             f2.write(str((62 - (count_node_rows))))
    //             f2.write(':')
    //             f2.write('\n')
    //             count_node_rows = count_node_rows + 1

    //         f2.write(str(all_samples_chip2[bit]))

    //         if bit == 440:
    //             break

    //         f2.write('\n')
    //         if (bit % 7)==6:
    //             f2.write('\n')
    //     f2.write('\n')

    int input_graph_array_chip1[101][101];
    // input_graph_array_chip1 = self.import_graph(input_graph_array_chip1, graph_problem_file)

    // ham_solution, majority_vote_array = graph_helper.cal_energy(graph_helper.cobifixed65_basegroups,1,self.selected_file,"run_files/chip2_test.txt",True,True)
    // ham_solution = ham_solution[0]
    // majority_vote_array = majority_vote_array[0]
    // acc = ham_solution/float((self.energy_ham))*100

    if(Verbose_ > 1) { // # calculate chip accuracy
    //     #print("Computed Hamiltonian for sample = " + str(ham_solution))
        printf("Hamiltonian of current sample = " + str(round(acc,2)) + "%\t", end="");  // # print chip accuracy
    //     #print("Spin Values (Ascending): " + str(majority_vote_array[0:size])+ '\n')
    }


    if (!return_spins) {
        return round(acc,2);
    } else {
        return round(acc,2), majority_vote_array;
    }
}

void ising_modify_array_for_pins(int **initial_array, int  **final_array, int problem_size)
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

    return final_pin_array;
}

// void ising_bit_stream_generate_adj(adj,i)
//     input_graph = graph_helper.realize_weights(graph_helper.cobifixed65_basegroups,adj)
//     input_graph = graph_helper.add_shil(input_graph,4)
//     input_graph = graph_helper.add_calibration(input_graph,"./calibration/cals.txt")
//     create_graph.write_prog_from_matrix(input_graph,i)
//     self.selected_file = "run_files/all_to_all_graph_write_%i.txt" %i
//     self.programming_bits = self.modify_array_for_pins(input_graph,np.zeros((64,64),dtype=np.int8),6

// py: cobifixed65_rpi::test_multi_times
void ising_test_multi_times(int sample_times)
//(self, sample_times, sample_bits, size, graph_problem_file,verbose=True,return_spins = False)
{
    ising_program_weights(/* TODO: how to pass around "py:self.program_weights" */);

    int times = 0;
    double *all_results = [];
    double cur_best = 0;
    double res;

    ising_cal_energy_ham(size, graph_problem_file) //# calculate Qbsolv energy once
    // ising_data_array = np.zeros((400,sample_times), dtype=np.int8)

    gpioWrite(ALL_ROW_HI, PI_HIGH);
    gpioWrite(SCANOUT_CLK, PI_LOW);
    gpioWrite(WEIGHT_EN, PI_HIGH);
    gpioWrite(WEIGHT_EN, PI_LOW);

    while (times < sample_times) {
        res = ising_cal_energy(sample_bits, size, times, ising_data_array, sample_times, graph_problem_file,verbose,return_spins);  //# calculate H energy from chip data
        if (res != None) {
            all_results.append(res);
            times += 1;
            // TODO: track cur_best
        }

        if (Verbose_ > 0) {
            printf(", best sample = %d %",  cur_best);
        }

    }

    gpioWrite(ALL_ROW_HI, PI_LOW);

    if (Verbose_ > 0) {
        printf("Finished!\n");
    }

    return all_results;
}
// def benchmark_adjacency(adj,sample_count):
//     board1 = cobifixed65_rpi()
//     board1.bit_stream_generate_adj(adj,0)
//     graph_problem_file = "{0}{1}.txt".format("run_files/all_to_all_graph_write_", 0)
//     result_tmp = board1.test_multi_times(sample_count, 7, 59, graph_problem_file, verbose=False,return_spins=True)
//     return result_tmp



int ising_init()
{
    if (gpioInitialise() < 0) return 1;

    // setup GPIO pins
    ising_gpio_setup();
}

bool ising_established()
{
    // TODO
    // connection = getenv("DW_INTERNAL__CONNECTION");
    // if (connection == NULL) {
    return false;
    // }
    // return true;
}

void ising_solver(double **val, int maxNodes, int8_t *Q)
{
    // TODO
}

void ising_close()
{
    gpioTerminate();
}

#ifdef __cplusplus
// }
#endif
