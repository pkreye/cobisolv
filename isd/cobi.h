
#define PCI_PROGRAM_LEN 166 // 166 * 64bits = (51 * 52 + 4(dummy bits)) * 4bits

#define COBI_DEVICE_NAME_LEN 22 // assumes 2 digit card number and null byte
#define COBI_DEVICE_NAME_TEMPLATE "/dev/cobi_pcie_card%hhu"

#define COBI_NUM_SPINS 46
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

typedef struct CobiInput{
    int Q[COBI_NUM_SPINS][COBI_NUM_SPINS];
    int debug;
} CobiInput;

typedef struct CobiOutput {
    int problem_id;
    int spins[COBI_NUM_SPINS];
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


// -- functions --
uint64_t swap_bytes(uint64_t val);
int bits_to_signed_int(uint8_t *bits, size_t num_bits);
void solveQ(int device, CobiInput* obj, CobiOutput *result);
int read_result(int device, CobiOutput *result, bool wait_for_result);
void submit_problem(int device, CobiInput *obj);
void init_device(int device);
int cobi_open_device(int device);
void cobi_write_program(int cobi_fd, uint64_t *program);
void cobi_reset(int cobi_fd);

int cobi_read(int cobi_fd, CobiOutput *result, bool wait_for_result);

int cobi_get_result_count(int cobi_fd);
bool cobi_has_result(int cobi_fd);
uint32_t cobi_read_status(int fd);

void cobi_wait_for_write(int cobi_fd);
void cobi_wait_for_read(int cobi_fd);
void print_spins(int *spins);
