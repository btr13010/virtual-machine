/*
    This program is a simulator for the LC-3 (Little Computer 3) architecture. 
    It is used to simulate the way computer hardware execute the LC-3 assembly code.
    The program will be run to execute a program file written in LC-3 assembly language.

    The small memory size of LC-3 is specified, along with the registers which are 
    used to store data and addresses in the memory during the execution of the program. 
    The registers include PC (program counter) which point toward the next instruction 
    to for the CPU to execute. The opcodes and their instructions are also provided 
    for the CPU to execute the commands.

    The program is written in the C language.
*/

#include <stdio.h>
#include <stdint.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/termios.h>
#include <sys/mman.h>

// Input buffer size
struct termios original_tio;

void disable_input_buffering() {
    /*
        This function disables the input buffering.
        Input buffering is a mechanism that allows the operating system to hold the input until the user presses the enter key.
        This function is used to disable the input buffering so that the program can read the input immediately.
    */

    tcgetattr(STDIN_FILENO, &original_tio);
    struct termios new_tio = original_tio;
    new_tio.c_lflag &= ~ICANON & ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
}

void restore_input_buffering() {
    /*
        This function restores the input buffering.
        This function is used to restore the input buffering after the program has finished reading the input.
    */

    tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
}

uint16_t check_key() {
    /*
        This function checks if the user has pressed a key.
        If the user has pressed a key, the function returns 1, otherwise it returns 0.
    */

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    return select(1, &readfds, NULL, NULL, &timeout) != 0;
}

// Create an array to store memory addresses
#define MEMORY_MAX (1 << 16) // using bitwise operation to define macro MEMORY_MAX = 2^16 = 65536
uint16_t memory[MEMORY_MAX];

/*
    Create an enum to store registers.
    The registers are used to store temporary data and addresses during the execution of the program. 
    The registers are stored inside CPU, so that it is faster to query data from registers.
    The CPU uses these data and addresses to perform operations.
    8 registers are used to store data: R0-R7, and 2 registers are used to store addresses: PC and COND.
*/
enum
{
    R_R0 = 0,
    R_R1,
    R_R2,
    R_R3,
    R_R4,
    R_R5,
    R_R6,
    R_R7,
    R_PC, // program counter: memory address of the next instruction to be executed
    R_COND, // condition flag: information about the previous operation
    R_COUNT // number of registers
};
uint16_t reg[R_COUNT]; // create an array to store the values of the registers

// Create an enum to store the set of 3 condition flags which indicate the sign of the previous calculation
enum
{
    FL_POS = 1 << 0, // the result of the previous calculation is positive
    FL_ZRO = 1 << 1, // the result of the previous calculation is zero
    FL_NEG = 1 << 2, // the result of the previous calculation is negative
};

// Create an enum to store the set of operations that the CPU can perform (opcodes)
enum
{
    OP_BR = 0, /* branch */
    OP_ADD, /* add  */
    OP_LD, /* load */
    OP_ST, /* store */
    OP_JSR, /* jump register */
    OP_AND, /* bitwise and */
    OP_LDR, /* load register */
    OP_STR, /* store register */
    OP_RTI, /* unused */
    OP_NOT, /* bitwise not */
    OP_LDI, /* load indirect */
    OP_STI, /* store indirect */
    OP_JMP, /* jump */
    OP_RES, /* reserved (unused) */
    OP_LEA, /* load effective address */
    OP_TRAP /* execute trap */
};

uint16_t swap16(uint16_t x) {
    /*
        The LC-3 is a little-endian architecture, but most modern computers are big-endian.
        In little-endian, the first byte is the least significant digit, and in big-endian, it is reversed.
        In order to swap a value from big-endian to little-endian, we need to reverse the order of the bytes.

        This function swaps the bytes of a 16-bit value to convert between big-endian and little-endian.
    */

    return (x << 8) | (x >> 8); // shift the first byte to the right by 8 bits and the second byte to the left by 8 bits
}

void read_image_file(FILE* file) {
    /*
        The assembly program is translated into a binary file called image file which is then loaded into a specific location in the memory.
        This function reads a 16-bit image file into memory at the location specified by the origin field in the file.

        The first two bytes of the file are the origin.
        The origin specifies the lowest address of the region of memory that is contained in the file.
        The rest of the file is a sequence of 16-bit big-endian values that make up the instructions and data for the program.
    */

    // the origin tells us where the image is placed in the memory
    uint16_t origin;
    fread(&origin, sizeof(origin), 1, file);
    origin = swap16(origin); // swap to little endian

    // we know the maximum file size so we only need one fread 
    uint16_t max_read = MEMORY_MAX - origin;
    uint16_t* p = memory + origin; // pointer to the memory location of the origin
    size_t read = fread(p, sizeof(uint16_t), max_read, file);

    // swap to little endian 
    while (read-- > 0) // check if read is greater than 0 then decrement it
    {
        *p = swap16(*p);
        ++p;
    }
}

int read_image(const char* image_path) {
    /*
        This function opens the image file and calls read_image_file to read the file.
    */

    FILE* file = fopen(image_path, "rb");
    if (!file) { return 0; };
    read_image_file(file);
    fclose(file);
    return 1;
}

// Create an enum to store the memory mapped registers which are not accessible from the normal register table
// Memory mapped registers are used to communicate with the special hardware devices
enum
{
    MR_KBSR = 0xFE00, // keyboard status register indicates if a key has been pressed
    MR_KBDR = 0xFE02  // keyboard data register indicates which key has been pressed
};

void mem_write(uint16_t address, uint16_t val) {
    /*
        This function writes a value to a memory address.
    */

    memory[address] = val;
}

uint16_t mem_read(uint16_t address) {
    /*
        This function reads a value from a memory address.
    */

    if (address == MR_KBSR) {
        if (check_key()) {
            memory[MR_KBSR] = (1 << 15);
            memory[MR_KBDR] = getchar();
        } else {
            memory[MR_KBSR] = 0;
        }
    }
    return memory[address];
}

void handle_interrupt(int signal) {
    /*
        This function handles the interrupt signal.
        When the user presses Ctrl+C, the program will terminate. 
    */

    restore_input_buffering();
    printf("\n");
    exit(-2);
}

int main(int argc, const char* argv[]) {
    /*
        The CPU has 3 main phases:
        1. Fetch: fetch the instruction from memory at the address of the PC register and increment the PC register
        2. Decode: decode the instruction by looking at the opcode to determine the operation to be performed
        3. Execute: execute the operation using the parameters in the instruction
    */

    // Handle command line inputs
    if (argc < 2) {
        /* show usage string */
        printf("lc3 [image-file1] ...\n");
        exit(2);
    }
    
    for (int j = 1; j < argc; ++j) {
        if (!read_image(argv[j])) {
            printf("failed to load image: %s\n", argv[j]);
            exit(1);
        }
    }

    // Setup
    signal(SIGINT, handle_interrupt);
    disable_input_buffering();

    /* since exactly one condition flag should be set at any given time, set the Z flag */
    reg[R_COND] = FL_ZRO;

    /* set the PC to starting position */
    /* 0x3000 is the default */
    enum { PC_START = 0x3000 };
    reg[R_PC] = PC_START;

    int running = 1;
    while (running)
    {
        /* FETCH */
        uint16_t instr = mem_read(reg[R_PC]++);
        uint16_t op = instr >> 12;

        switch (op)
        {
            case OP_ADD:
                @{ADD}
                break;
            case OP_AND:
                @{AND}
                break;
            case OP_NOT:
                @{NOT}
                break;
            case OP_BR:
                @{BR}
                break;
            case OP_JMP:
                @{JMP}
                break;
            case OP_JSR:
                @{JSR}
                break;
            case OP_LD:
                @{LD}
                break;
            case OP_LDI:
                @{LDI}
                break;
            case OP_LDR:
                @{LDR}
                break;
            case OP_LEA:
                @{LEA}
                break;
            case OP_ST:
                @{ST}
                break;
            case OP_STI:
                @{STI}
                break;
            case OP_STR:
                @{STR}
                break;
            case OP_TRAP:
                @{TRAP}
                break;
            case OP_RES:
            case OP_RTI:
            default:
                @{BAD OPCODE}
                break;
        }
    }

    // When the program is interrupted, the terminal settings is restored back to normal.
    restore_input_buffering();
}
