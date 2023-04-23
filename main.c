#include <stdio.h>
#include <stdint.h>

// Create an array to store memory addresses
#define MEMORY_MAX (1 << 16) // use 16 bits to store the memory address => MEMORY_MAX = 2^16 = 65536
uint16_t memory[MEMORY_MAX];

/*
    Create an enum to store registers. 
    The registers are used to store data and addresses during the execution of the program. 
    The CPU uses the registers to perform operations on the data and addresses.
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
    R_COUNT
};
uint16_t reg[R_COUNT];

// Create an enum to store opcodes
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