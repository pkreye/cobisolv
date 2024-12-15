/*
 Copyright 2017 D-Wave Systems Inc.
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

#ifdef _WIN32
	#include "wingetopt.h"
#else
	#include <getopt.h>
#endif // _WIN32

#include "cobi.h"
#include "qbsolv.h"
#include "readqubo.h"
#include "util.h"


// Debug getopt long parmeters
#define GETOPT_PID            1000
#define GETOPT_DCO            1001
#define GETOPT_SDELAY         1002
#define GETOPT_MFAIL          1003
#define GETOPT_ROSC           1004
#define GETOPT_SHIL_TIME      1005
#define GETOPT_WEIGHT_TIME    1006
#define GETOPT_STIME          1007
#define GETOPT_CARD           1008
#define GETOPT_SHIL_VAL       1009

void print_help(void);
void print_qubo_format(void);

// define what will be used by "extern" in other functions
//
FILE *outFile_;
FILE *solution_input_;
int maxNodes_, nCouplers_, nNodes_, findMax_, numsolOut_;
int Verbose_, TargetSet_, WriteMatrix_, Tlist_;
char *outFileNm_, pgmName_[16], algo_[4];
double **val;
double Target_, Time_;
struct nodeStr_ *nodes_;
struct nodeStr_ *couplers_;

//  main routine,
//  -read the command line
//  -read the input qubo file
//  -setup the matrix
//  -call the solver
//  //  put in large statics like the -help output
//      and the qubo file format print out
//

const int defaultRepeats = 50;

int main(int argc, char *argv[]) {
    /*
     *  Initialize global variables, set up data structures,
     *  process the commandline and the environment, ...
     *  Initialize global variables
     */

    parameters_t param = default_parameters();

    bool delimitedOutput = false;

    // Sub sampler flags
    bool use_cobi = false;
    bool use_rand  = false;
    bool use_null  = false;

    extern char *optarg;
    extern int optind, optopt, opterr;

    char *inFileName = NULL;
    FILE *inFile = NULL;

    strcpy(pgmName_, "cobisolv");
    findMax_ = false;
    Verbose_ = 0;
    strcpy(algo_, "o");  // algorithm default

    WriteMatrix_ = false;
    outFile_ = stdout;
    TargetSet_ = false;
    Time_ = 2592000;  // the maximum runtime of the algorithm in seconds before timeout (2592000 = a month's worth of
                      // seconds)
    Tlist_ = -1;      // tabu list length  -1 signals go with defaults

    // int64_t seed = 17932241798878;  moved to param struct

    int errorCount = 0;

    static struct option longopts[] = {{"help", no_argument, NULL, 'h'},
                                       {"infile", required_argument, NULL, 'i'},
                                       {"outfile", required_argument, NULL, 'o'},
                                       {"verbosityLevel", required_argument, NULL, 'v'},
                                       {"version", no_argument, NULL, 'V'},
                                       {"subproblemSize", required_argument, NULL, 'S'},
                                       {"target", required_argument, NULL, 'T'},
                                       {"repeats", required_argument, NULL, 'n'},
                                       {"solutionIn", required_argument, NULL, 's'},
                                       {"max", no_argument, NULL, 'm'},
                                       {"output", required_argument, NULL, 'o'},
                                       {"timeout", required_argument, NULL, 't'},
                                       {"quboFormat", no_argument, NULL, 'q'},
                                       {"tlist", required_argument, NULL, 'l'},
                                       {"seed", required_argument, NULL, 'r'},
                                       {"Algo", required_argument, NULL, 'a'},
                                       {"preSearchPassFactor", required_argument, NULL, 'p'},
                                       {"globalSearchPassFactor", required_argument, NULL, 'g'},
                                       {"useCobi", no_argument, NULL, 'C'},
                                       {"delimitedOutput", no_argument, NULL, 'd'},
                                       {"cobiNumSamples", required_argument, NULL, 'z'},
                                       {"numOutputSolutions", required_argument, NULL, 'N'},
                                       {"parallelCobi", optional_argument, NULL, 'P'},
                                       {"evalStrat", required_argument, NULL, 'e'},
                                       // Debug params
                                       {"pid", required_argument, NULL, GETOPT_PID},
                                       {"dco", required_argument, NULL, GETOPT_DCO},
                                       {"sdelay", required_argument, NULL, GETOPT_SDELAY},
                                       {"mfail", required_argument, NULL, GETOPT_MFAIL},
                                       {"rosc", required_argument, NULL, GETOPT_ROSC},
                                       {"shilTime", required_argument, NULL, GETOPT_SHIL_TIME},
                                       {"weightTime", required_argument, NULL, GETOPT_WEIGHT_TIME},
                                       {"stime", required_argument, NULL, GETOPT_STIME},
                                       {"card", required_argument, NULL, GETOPT_CARD},
                                       {"shilVal", required_argument, NULL, GETOPT_SHIL_VAL},

                                       //

                                       {NULL, no_argument, NULL, 0}};

    int opt, option_index = 0;
    char *chx;               // used as exit ptr in strto(x) functions

    while ((opt = getopt_long(argc, argv, "Hhi:o:v:VS:T:l:n:wmo:t:qr:a:p:g:Cdz:N:P::e:", longopts, &option_index)) != -1) {
        switch (opt) {
            case 'a':
                strcpy(algo_, optarg);  // algorithm copied off of command line -a option
                switch (algo_[0]) {
                    case 'o':
                        // Original Dwave algorithm
                        break;
                    case 'd':
                        // choose "Solution diversity"
                        break;
                    case 'b':
                        // BFS decomposer
                        break;
                    default: /* unknown */
                        printf(" Unknown Algorithm choice: options are o:d cmdline had %s \n", algo_);
                        exit(9);
                        break;
                }
                break;
            case 'H':
            case 'h':
                print_help();
                exit(0);
            case 'i':
                inFileName = optarg;

                strncpy(param.problemName, inFileName, 24);
                param.problemName[24] = '\0';

                if ((inFile = fopen(inFileName, "r")) == NULL) {
                    fprintf(stderr,
                            "\n\t Error - can't find/open file "
                            "\"%s\"\n\n",
                            optarg);
                    exit(9);
                }
                break;
            case 'l':
                Tlist_ = strtol(optarg, &chx, 10);  // this sets the length of the tabu list
                break;
            case 'm':
                findMax_ = true;  // go for the maximum value otherwise the minimum is found by default
                break;
            case 'n':
                param.repeats =
                        strtol(optarg, &chx, 10);  // this sets the number of outer loop repeats without improvement
                break;
            case 'v':
                Verbose_ = strtol(optarg, &chx, 10);  // this sets the value of the Verbose
                break;
            case 'V':
                fprintf(outFile_, " Version " VERSION " \n Compiled: " __DATE__
                                  ","__TIME__
                                  "\n");
                exit(9);
                break;
            case 'S':
                param.sub_size = strtol(optarg, &chx, 10);  // this sets the size of the Partitioning Matrix
                if (param.sub_size < 10) {
                    // other settings, this could be setting it from true above to false here.
                    if (param.sub_size != 0) {
                        fprintf(stderr, "\n Error --  SubMatrix must be 0 or greater than 10.  -S %d\n ", param.sub_size);
                        ++errorCount;
                    }
                }

                use_cobi = false;
                /* if (param.sub_size == 0) { */
                /*     use_cobi = true;  // except where -S 0 */
                /* } */
                break;
            case 'T':
                Target_ = strtod(optarg, (char **)NULL);  // this sets desired optimal energy
                TargetSet_ = true;
                break;
            case 't':
                Time_ = strtof(optarg, (char **)NULL);  // this sets the maximum runtime of the algorithm in seconds
                break;
            case 'o':
                if ((outFile_ = fopen(optarg, "w")) == NULL) {
                    fprintf(stderr,
                            "\n\t Error - can't find/write file "
                            "\"%s\"\n\n",
                            optarg);
                    exit(9);
                }
                outFileNm_ = optarg;
                break;
            case 'q':
                print_qubo_format();
                exit(0);
                break;
            case 'r':
                param.seed = strtol(optarg, &chx, 10);  // sets the seed value
                break;
            case 'w':
                WriteMatrix_ = true;
                break;
            case 'p':
                // Overwrite initial global tabu search pass factor
                param.preSearchPassFactor = strtol(optarg, &chx, 10);
                /* printf("Test: %ld, %d\n", param.preSearchPassFactor, param.preSearchPassFactor); */
                break;
            case 'g':
                // Overwrite the repeated global tabu search pass factor
                param.globalSearchPassFactor = strtol(optarg, &chx, 10);
                break;
            case 'C':
                use_cobi = true;
                break;
            case 'd':
                delimitedOutput = true;
                break;
            case 'N':
                // Number of solutions to output
                param.num_output_solutions = strtol(optarg, &chx, 10);
                break;
            case 'z':
                // Number of samples to take when solving a subqubo with the cobi chip
                param.cobi_num_samples = strtol(optarg, &chx, 10);
                break;
            case 'D':
                param.cobi_descend = true;
                break;

            case 'e':
                /* printf("Eval strat input \"%s\"\n", optarg); */
                param.cobi_eval_strat = cobi_parse_eval_strat(optarg);
                /* printf("strat: %d\n", param.eval_strat); */
                /* exit(1); */
                break;

            case 'P':
                // Device parallelism currently not supported with COBIFIVE
                /* param.cobi_parallel_repeat = true; // */
                /* param.num_sub_prob_threads = -1; */
                /* // Handle whitespace before optional argument */
                /* if (optarg == NULL && optind < argc && argv[optind][0] != '-') */
                /* { */
                /*     optarg = argv[optind++]; */
                /* } */

                /* if (optarg != NULL) { */
                /*     param.num_sub_prob_threads = strtol(optarg, &chx, 10); */
                /* } */
                break;


            // debug
            case GETOPT_PID:
                param.pid = (uint16_t) strtol(optarg, &chx, 10);
                break;
            case GETOPT_DCO:
                param.dco = (uint16_t)strtol(optarg, &chx, 10);
                break;
            case GETOPT_SDELAY:
                param.sample_delay = (uint16_t)strtol(optarg, &chx, 10);
                break;
            case GETOPT_MFAIL:
                param.max_fails = (uint16_t)strtol(optarg, &chx, 10);
                break;
            case GETOPT_ROSC:
                param.rosc_time = (uint16_t)strtol(optarg, &chx, 10);
                break;
            case GETOPT_SHIL_TIME:
                param.shil_time = (uint16_t)strtol(optarg, &chx, 10);
                break;
            case GETOPT_WEIGHT_TIME:
                param.weight_time = (uint16_t)strtol(optarg, &chx, 10);
                break;
            case GETOPT_STIME:
                param.sample_time = (uint16_t)strtol(optarg, &chx, 10);
                break;
            case GETOPT_CARD:
                param.cobi_card_num = (uint16_t)strtol(optarg, &chx, 10);
                break;
            case GETOPT_SHIL_VAL:
                param.shil_val = (uint8_t)strtol(optarg, &chx, 10);
                break;

            default: /* '?' or unknown */
                print_help();
                exit(0);
                break;
        }
    }
    // options from command line complete
    //
    srand(param.seed);

    if (inFile == NULL) {
        fprintf(stderr,
                "\n\t%s error -- no input file (-i option) specified"
                "\n\n",
                pgmName_);
        exit(9);
    }

    errorCount = read_qubo(inFileName, inFile);  // read in the QUBO from file

    if ((errorCount > 0)) {
        fprintf(stderr,
                "\n\t%d Input error(s) on file \"%s\"\n\n"
                "\t%s has been stopped.\n\tThere is a description "
                "of the .qubo file format: use %s -q to print it\n\n",
                errorCount, inFileName, pgmName_, pgmName_);
        exit(1);
    }

    val = (double **)malloc2D(maxNodes_, maxNodes_, sizeof(double));    // create a 2d double array
    fill_qubo(val, maxNodes_, nodes_, nNodes_, couplers_, nCouplers_);  // move to a 2d array

    numsolOut_ = 0;

    if (use_cobi) {
        // If a specific card is requested, restrict to that card
        if (param.cobi_card_num >= 0) {
            param.num_sub_prob_threads = 1;
        }

        if (param.num_sub_prob_threads == 0) {
            fprintf(stderr, "Requested number of devices must be greater than 0.\n");
            exit(2);
        } else if (param.num_sub_prob_threads != 1) {
            // force use of bfs decomposer when using multiple devices
            strcpy(algo_, "b");
        }

        if (cobi_init(&param.num_sub_prob_threads, param.cobi_card_num) != 0) {
            fprintf(stderr, "init failed\n");
            exit(1);
        }

        if (atexit(cobi_close) != 0) {
            fprintf(stderr, "failed to register exit function\n");
            exit(1);
        }

        if (param.cobi_num_samples < 1) {
            fprintf(stderr, "number of samples must be greater than 0.\n");
            exit(2);
        }

        param.use_cobi = true;
        param.sub_size = COBI_NUM_SPINS;
        param.sub_sampler = &cobi_sub_sample;
        param.sub_sampler_data = &param;
    } else {
        // stick to single thread when not using cobi boards
        param.num_sub_prob_threads = 1;
    }

    if (use_rand) {
        param.sub_sampler = &rand_sub_sample;
    }

    if (use_null) {
        param.sub_sampler = &null_sub_sample;
    }


    // get some memory for storing and shorting Q bit vectors
    int QLEN = 20;  // the max number of solutions to store in soltuion_lists
    if (strncmp(&algo_[0], "o", strlen("o")) == 0) {
        QLEN = 20;  // don't need a big que for this optimization
    } else if (strncmp(&algo_[0], "d", strlen("d")) == 0) {
        QLEN = 75;  // this need a lot of diversity
    }

    int8_t **solution_list;
    double *energy_list;
    int *solution_counts, *Qindex;

    solution_list = (int8_t **)malloc2D(QLEN + 1, maxNodes_, sizeof(int8_t));
    if (GETMEM(energy_list, double, QLEN + 1) == NULL) BADMALLOC
    if (GETMEM(solution_counts, int, QLEN + 1) == NULL) BADMALLOC
    if (GETMEM(Qindex, int, QLEN + 1) == NULL) BADMALLOC

    solve(val, maxNodes_, solution_list, energy_list, solution_counts, Qindex, QLEN, &param, delimitedOutput);

    free(solution_list);
    free(energy_list);
    free(solution_counts);
    free(Qindex);
    free(val);

    /* // should have been registered via atexit */
    /* if (use_cobi) { */
    /*     cobi_close(); */
    /* } */

    if (Verbose_ > 3) {
        fprintf(outFile_, "\n\t\"cobisolv  -i %s\" (%d nodes, %d couplers) - end-of-job\n\n", inFileName, nNodes_,
                nCouplers_);
    }
    exit(0);
}

void print_help(void) {
    printf("\n\t%s -i infile [-C] [-P numDevices] [-d] [-z numSamples] [-p preSearchFactor] \n"
           "\t\t[-g globalSearchPassFactor] [-o outfile] [-m] [-T] [-n] [-S SubMatrix] \n"
           "\t\t[-w] [-h] [-a algorithm] [-v verbosityLevel] [-V] [-q] [-t seconds]\n"
           "\nDESCRIPTION\n"
           "\tcobisolv executes a quadratic unconstrained binary optimization \n"
           "\t(QUBO) problem represented in a file, providing bit-vector \n"
           "\tresult(s) that minimizes (or optionally, maximizes) the value of \n"
           "\tthe objective function represented by the QUBO.  The problem is \n"
           "\trepresented in the QUBO(5) file format and notably is not limited \n"
           "\tto the size or connectivity pattern of the D-Wave system on which \n"
           "\tit will be executed. \n"
           "\t\tThe options are as follows: \n"
           "\t-i infile \n"
           "\t\tThe name of the file in which the input QUBO resides.  This \n"
           "\t\tis a required option. \n"
           "\t-C \n"
           "\t\tIf present, this optional argument causes subproblems to be\n"
           "\t\tsolved on the COBI chip. Without this option subproblems will\n"
           "\t\tbe solved with tabu search.\n"
           "\t-P numDevices\n"
           "\t\tRun each subproblem in parallel on all available COBI devices.\n"
           "\t\tThis option must be used in conjunction with the \"-C\" flag.\n"
           "\t\tIf an optional positive integer is provided, an attempt will be\n"
           "\t\tmade to use that many COBI devices or as many as are available.\n"
           "\t-z numSamples \n"
           "\t\tOptional argument that sets the number of solutions to\n"
           "\t\tsample from the cobi chip for each subproblem. Defaults to 10.\n"
           "\t-N numOutputSolutions\n"
           "\t\tOptional argument that sets the number of unique solutions to be output.\n"
           "\t\tIf the value provided is 0, all unique solutions will be output.\n"
           "\t-p preSearchPassFactor\n"
           "\t\tScale the amount of classical tabu search to be performed\n"
           "\t\tbefore decomposing problem to subproblems. Defaults to 0.\n"
           "\t\tStandard qbsolv uses 6500.\n"
           "\t-g globalSearchPassFactor\n"
           "\t\tScale the amount of classical tabu search to be performed\n"
           "\t\tbetween each round of subproblem solutions. Defaults to 0.\n"
           "\t\tStandard qbsolv uses 1700.\n"
           "\t-d\n"
           "\t\tIf this optional argument is present, the final output will be\n"
           "\t\tprinted as comma delimited data. Allowing for aggregation of\n"
           "\t\tmultiple runs in a single data file.\n"
           "\t-o outfile \n"
           "\t\tThis optional argument denotes the name of the file to \n"
           "\t\twhich the output will be written.  The default is the \n"
           "\t\tstandard output. \n"
           "\t-a algorithm \n"
           "\t\t This optional argument chooses nuances of the outer loop\n"
           "\t\t algorithm.  The default is o.\n"
           "\t\t \'o\' for original qbsolv method. Submatrix based upon change in energy.\n"
           "\t\t \'d\' for solution diversity. Submatrix based upon differences of solutions\n"
           "\t\t \'b\' for BFS decomposer. Submatrix based upon breadth-first search.\n"
           "\t-m \n"
           "\t\tThis optional argument denotes to find the maximum instead \n"
           "\t\tof the minimum. \n"
           "\t-T target \n"
           "\t\tThis optional argument denotes to stop execution when the \n"
           "\t\ttarget value of the objective function is found. \n"
           "\t-t timeout \n"
           "\t\tThis optional argument stops execution when the elapsed \n"
           "\t\tcpu time equals or exceeds timeout value. Timeout is only checked \n"
           "\t\tafter completion of the main loop. Other halt values \n"
           "\t\tsuch as \'target\' and \'repeats\' will halt before \'timeout\'.\n"
           "\t\tThe default value is %8.1f.\n"
           "\t-n repeats \n"
           "\t\tThis optional argument denotes, once a new optimal value is \n"
           "\t\tfound, to repeat the main loop of the algorithm this number\n"
           "\t\tof times with no change in optimal value before stopping.  \n"
           "\t\tThe default value is %d. \n"
           "\t-S subproblemSize \n"
           "\t\tThis optional argument indicates the size of the sub-\n"
           "\t\tproblems into which the QUBO will be decomposed.  A \n"
           "\t\t\"-S 0\" or \"-S\" argument not present indicates to\n"
           "\t\tuse the default value (47) for subproblem sizes.\n"
           "\t-s  solutionIn   \n"
           "\t\tIf present, this optional argument is a filename that is\n"
           "\t\tin the format of the first 2 records of the output solution file,\n"
           "\t\tthe solution read from this file will be the initial solution used\n"
           "\t\tin the solver\n"
           "\t-w \n"
           "\t\tIf present, this optional argument will print the QUBO \n"
           "\t\tmatrix and result in .csv format. \n"
           "\t-h \n"
           "\t\tIf present, this optional argument will print the help or \n"
           "\t\tusage message for cobisolv and exit without execution. \n"
           "\t-v verbosityLevel \n"
           "\t\tThis optional argument denotes the verbosity of output. A \n"
           "\t\tverbosityLevel of 0 (the default) will output the number of \n"
           "\t\tbits in the solution, the solution, and the energy of the \n"
           "\t\tsolution.  A verbosityLevel of 1 will output the same \n"
           "\t\tinformation for multiple solutions, if found. A \n"
           "\t\tverbosityLevel of 2 will also output more detailed \n"
           "\t\tinformation at each step of the algorithm. This increases   \n"
           "\t\tthe output up to a value of 4.\n"
           "\t-V \n"
           "\t\tIf present, this optional argument will emit the version \n"
           "\t\tnumber of the cobisolv program and exit without execution. \n"
           "\t-q \n"
           "\t\tIf present, this optional argument triggers printing the \n"
           "\t\tformat of the QUBO file.\n"
           "\t-r seed \n"
           "\t\tUsed to reset the seed for the random number generation \n",
           pgmName_, Time_, defaultRepeats);

    return;
}

void print_qubo_format(void) {
    const char *quboFormat =
            "\n   A .qubo file contains data which describes an unconstrained\n"
            "quadratic binary optimization problem.  It is an ASCII file comprise"
            "d\nof four types of lines:\n\n"
            "1) Comments - defined by a \"c\" in column 1.  They may appear\n"
            "\tanywhere in the file, and are otherwise ignored.\n\n"
            "2) One program line, which starts with p in the first column.\n"
            "\tThe program line must be the first non-comment line in the file.\n"
            "\tThe program line has six required fields (separated by space(s)),\n"
            "\tas in this example:\n\n"
            "  p   qubo  topology   maxNodes   nNodes   nCouplers\n\n"
            "    where:\n"
            "  p          the problem line sentinel\n"
            "  qubo       identifies the file type\n"
            "  topology   a string which identifies the topology of the problem\n"
            "\t     and the specific problem type.  For an unconstrained problem,\n"
            "\t     target will be \"0\" or \"unconstrained.\"   Possible, for future\n"
            "\t     implementations, valid strings might include \"chimera128\"\n"
            "\t     or \"chimera512\" (among others).\n"
            "  maxNodes   number of nodes in the topology.\n"
            "  nNodes     number of nodes in the problem (nNodes <= maxNodes).\n"
            "\t     Each node has a unique number and must take a value in the\n"
            "\t     the range {0 - (maxNodes-1)}.  A duplicate node number is an\n"
            "\t     error.  The node numbers need not be in order, and they need\n"
            "\t     not be contiguous.\n"
            "  nCouplers  number of couplers in the problem.  Each coupler is a\n"
            "\t     unique connection between two different nodes.  The maximum\n"
            "\t     number of couplers is (nNodes)^2.  A duplicate coupler is\n"
            "\t     an error.\n\n"
            "3) nNodes clauses.  Each clause is made up of three numbers.  The\n"
            "\tnumbers are separated by one or more blanks.  The first two\n"
            "\tnumbers must be integers and are the number for this node\n"
            "\t(repeated).  The node number must be in {0 , (maxNodes-1)}.\n"
            "\tThe third value is the weight associated with the node, may be\n"
            "\tan integer or float, and can take on any positive or negative\n"
            "\tvalue, or zero.\n\n"
            "4) nCouplers clauses.  Each clause is made up of three numbers.  The\n"
            "\tnumbers are separated by one or more blanks.  The first two\n"
            "\tnumbers must be different integers and are the node numbers\n"
            "\tfor this coupler.  The two values (i and j) must have (i < j).\n"
            "\tEach number must be one of the nNodes valid node numbers (and\n"
            "\tthus in {0, (maxNodes-1)}).  The third value is the strength\n"
            "\tassociated with the coupler, may be an integer or float, and can\n"
            "\ttake on any positive or negative value, but not zero.  Every node\n"
            "\tmust connect with at least one other node (thus must have at least\n"
            "\tone coupler connected to it).\n\n"
            "Here is a simple QUBO file example for an unconstrained QUBO with 4\n"
            "nodes and 6 couplers.  This example is provided to illustrate the\n"
            "elements of a QUBO benchmark file, not to represent a real problem.\n"
            "\n\t\tc\n"
            "\t\tc  This is a sample .qubo file\n"
            "\t\tc  with 4 nodes and 6 couplers\n"
            "\t\tc\n"
            "\t\tp  qubo  0  4  4  6 \n"
            "\t\tc ------------------\n"
            "\t\t0  0   3.4\n"
            "\t\t1  1   4.5\n"
            "\t\t2  2   2.1\n"
            "\t\t3  3   -2.4\n"
            "\t\tc ------------------\n"
            "\t\t0  1   2.2\n"
            "\t\t0  2   3.4\n"
            "\t\t1  2   4.5\n"
            "\t\t0  3   -2\n"
            "\t\t1  3   4.5678\n"
            "\t\t2  3   -3.22\n\n";

    printf("%s", quboFormat);
    return;
}
