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

#include "utils.h"

// Create an array to store memory addresses
#define MEMORY_MAX (1 << 16) // using bitwise operation to define macro MEMORY_MAX = 2^16 = 65536
uint16_t memory[MEMORY_MAX];

/*
    Create an array reg to store register values. 
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
uint16_t reg[R_COUNT]; 

// Create an enum to store the set of 3 condition flags which indicate the sign of the previous calculation
enum
{
    FL_POS = 1 << 0, // P: the result of the previous calculation is positive
    FL_ZRO = 1 << 1, // Z: he result of the previous calculation is zero
    FL_NEG = 1 << 2, // N: the result of the previous calculation is negative
};

// The set of operations that the CPU can perform (opcodes) 
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

void read_image_file(FILE* file) {
    /*
        The assembly program is translated into a binary file called image file which is then loaded into a specific location in the memory.
        This function reads a 16-bit image file and store it into memory at the location specified by the origin field in the file.

        The first two bytes of the file are the origin.
        The origin specifies the lowest address of the region of memory that is contained in the file.
        The rest of the file is a sequence of 16-bit big-endian values that make up the instructions and data for the program.
    */

    // the origin at the start of the file tells us where the image is placed in the memory
    uint16_t origin;
    fread(&origin, sizeof(origin), 1, file); // read origin
    origin = swap16(origin); // swap to little endian

    uint16_t max_read = MEMORY_MAX - origin; // maximum number of words we can read in case the file is too big
    uint16_t* p = memory + origin; // pointer to the memory location of the origin (start of the image file)
    // read the file into memory, from the pointer p
    size_t read = fread(p, sizeof(uint16_t), max_read, file); // read is the number of words in the file

    // swap all the words to little endian 
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

    // check if the address is a keyboard status register
    if (address == MR_KBSR) {
        if (check_key()) { // if user pressed a key
            memory[MR_KBSR] = (1 << 15); // set MR_KBSR to 1 indicating a key is ready to be read
            memory[MR_KBDR] = getchar(); // set MR_KBDR to the key that was pressed
        } else {
            memory[MR_KBSR] = 0; // set MR_KBSR to 0 indicating there is no key to be read
        }
    }
    return memory[address];
}

// Create an enum to store the set of trap codes
enum
{
    TRAP_GETC = 0x20,  /* get character from keyboard, not echoed onto the terminal */
    TRAP_OUT = 0x21,   /* output a character */
    TRAP_PUTS = 0x22,  /* output a word string */
    TRAP_IN = 0x23,    /* get character from keyboard, echoed onto the terminal */
    TRAP_PUTSP = 0x24, /* output a byte string */
    TRAP_HALT = 0x25   /* halt the program */
};

void update_flags(uint16_t r) {
    /*
        This function updates the condition flags based on the value of the register.
    */

    if (reg[r] == 0) {
        reg[R_COND] = FL_ZRO;
    } else if (reg[r] >> 15) { // a 1 in the left-most bit indicates negative
        reg[R_COND] = FL_NEG;
    } else {
        reg[R_COND] = FL_POS;
    }
}

int main(int argc, const char* argv[]) {
    /*
        The CPU has 3 main phases:
        1. Fetch: fetch the instruction from memory at the address of the PC register and increment the PC register
        2. Decode: decode the instruction by looking at the opcode to determine the operation to be performed
        3. Execute: execute the operation using the parameters in the instruction

        Input:
            argc: the number of elements in the argv string array
            *argv: an array of strings containing the paths to the image files
    */

    // Preprocess command line inputs
    if (argc < 2) {
        /* show usage string */
        printf("lc3 [image-file1] ...\n");
        exit(2);
    }
    // read the image files into memory and exit if any of the files fail to load
    for (int j = 1; j < argc; ++j) {
        if (!read_image(argv[j])) 
        {
            printf("failed to load image: %s\n", argv[j]);
            exit(1);
        }
    }

    // Setup
    signal(SIGINT, handle_interrupt);
    disable_input_buffering();

    // Initialize the condition flag to Z
    reg[R_COND] = FL_ZRO;

    // Set the PC to the starting position (default 0x3000)
    enum { PC_START = 0x3000 };
    reg[R_PC] = PC_START;

    int running = 1;
    while (running) {
        /* Main loop */

        // read the instruction from memory at the address of the PC register and increment the PC register
        uint16_t instr = mem_read(reg[R_PC]++); 
        uint16_t op = instr >> 12; // the instruction is located at the left-most 4 bits

        switch (op) {
            case OP_ADD:
                {
                    // desReg: the destination of register
                    uint16_t desReg = (instr >> 9) & 0b111;
                    // SR1: The first operand (SR1)
                    uint16_t SR1 = (instr >> 6) & 0b111;
                    //Check if it is in immediate value mode (bit[5]==1)
                    uint16_t imm_mode = (instr >> 5) & 0b1;

                    /* 
                    If it is in the immidiate value mode, 
                        the second source operand is obtained
                            by sign-extending to 16 bits. 
                    If it is not in the immidiate value mode,
                        the second operand is optained 
                            from SR2. 
                    This case is an "ADD" operator, so it adds the two operands using "+".
                    */
                    reg[desReg] = reg[SR1] + (imm_mode ? sign_extend(instr & 0b11111, 5) : reg[instr & 0b111]);
                    

                    update_flags(desReg);

                }
                break;
       
            case OP_AND:
                {
                    // desReg: the destination of register
                    uint16_t desReg = (instr >> 9) & 0b111;
                    // SR1: The first operand (SR1)
                    uint16_t SR1 = (instr >> 6) & 0b111;
                    //Check if it is in immediate value mode (bit[5]==1)
                    uint16_t imm_mode = (instr >> 5) & 0b1;
                
                    /* 
                    If it is in the immidiate value mode, 
                        the second source operand is obtained
                            by sign-extending to 16 bits. 
                    If it is not in the immidiate value mode,
                        the second operand is optained 
                            from SR2. 
                    Similarly, this case is an "AND" operator, so it uses &.
                    */
                    reg[desReg] = reg[SR1] & (imm_mode ? sign_extend(instr & 0b11111, 5) : reg[instr & 0b111]);

                    update_flags(desReg);
                }
                break;

            case OP_NOT:
                {
                    // desReg: the destination of register
                    uint16_t desReg = (instr >> 9) & 0b111;
                    // SR1: The first operand (SR1)
                    uint16_t SR1 = (instr >> 6) & 0b111;

                    /*
                    The ~ (bitwise NOT) in C or C++
                    takes one number and inverts all bits of it.
                    */
                    reg[desReg] = ~reg[SR1];
                    
                    update_flags(desReg);
                }
                break;

            case OP_BR:
                {   
                    //Sign-extend the PC offset of 9 bits
                    uint16_t PCoffset9 = sign_extend(instr & 0b111111111, 9);
                    uint16_t conditions_9 = (instr >> 9) & 0b111;
                    /*
                    The conditions are identified by the state of bits [11:9].
                    If any of the condition codes tested is set
                    (conditions != 0), increase the PC with the
                    sign-extended PCoffset.
                    (The branch is taken if the below condition is true)
                    */
                    if (conditions_9 & reg[R_COND])
                    {
                        reg[R_PC] = reg[R_PC] + PCoffset9;
                    }
                }
                break;

            case OP_JMP:
                {
                    // SR1: The first operand (SR1)
                    uint16_t SR1 = (instr >> 6) & 0b111;
                    /* 
                    This operator makes the program unconditionally 
                    jumps to the location specified in the base register.
                    The base register is identified at bits[8:6].
                    This also handles RET since RET is a special case of JMP,
                    happens when SR1 is 7.
                    */
                    reg[R_PC] = reg[SR1];
                }
                break;

            case OP_JSR:
                {
                    /*
                        The condition at bit[11] specifies 2 cases of the JSR operation.

                        JSRR case: If bit[11] = 0, the address of the subroutine is obtained from the base register.
                            Example in assembly code:
                                JSRR R2 ; Store the next PC address to R7, then jump to the address stored in R2.

                        JSR case: If bit[11] = 1, the address is computed by sign-extending bits [10:0]
                            (PC offset is of 11 bits) and adding it to the incremented PC.
                            Example in assembly code:
                                JSR LOOP ; Store the next PC address to R7, then jump to LOOP.
                    */

                    // Get the condition at bit[11]
                    uint16_t conditions_11 = (instr >> 11) & 0b1;
                    // Store the PC address to R7
                    reg[R_R7] = reg[R_PC];

                    if (conditions_11) //JSR
                    {
                        uint16_t PCoffset11 = sign_extend(instr & 0b11111111111, 11);
                        reg[R_PC] = reg[R_PC] + PCoffset11;  
                    }
                    else //JSRR
                    {
                        uint16_t SR1 = (instr >> 6) & 0b111;
                        reg[R_PC] = reg[SR1]; //SR1 is the base register
                    }
                }
                break;
            
            case OP_LD:
                {
                    // desReg: the destination of register is the left-most 3 bits after the opcode
                    // desReg stores the result of the operation
                    uint16_t desReg = (instr >> 9) & 0b111;
                    // Sign-extend the PC offset which is stored in the right-most 9 bits
                    uint16_t PCoffset9 = sign_extend(instr & 0b111111111, 9);

                    /*
                        The LD operation loads the address which is calculated by sign-extending the bits[8:0], 
                        and then adding this value to the incremented PC.
                        desReg is loaded with the information from memory located at this address.
                            Example in assembly code:
                                LD R0, LOOP ; R0 <- mem_read(LOOP)
                    */
                    reg[desReg] = mem_read(reg[R_PC] + PCoffset9);
                    update_flags(desReg);
                }
                break;

            case OP_LDI:
                {   
                    /*
                        Load Indirect: load a value from address to a register. The address from which value is extracted can be 
                        calculated by adding the sign-extended of the rightmost 9 bits to the incremented program counter (PC).
                            Example in assembly code:
                                LDI R0, LOOP ; R0 <- mem_read(mem_read(LOOP))
                    */
                    
                    uint16_t DR = (instr >> 9) & 0b111;  // destination register (DR) is specified by bits [11:9]
                    uint16_t PCoffset9 = instr & 0b111111111;  // PCoffset9 is specified by the rightmost 9 bits.

                    // add the sign-extended value of PCoffset9 to the current PC to calculate the address where the value will be taken, load it to the destination register. 
                    reg[DR] = mem_read(mem_read(reg[R_PC] + sign_extend(PCoffset9, 9)));
                    update_flags(DR);
                }
                break;
            case OP_LDR:
                {
                    /*
                        Load Base+offset: Assign value from an address to destination register which is specified by bits [11:9].
                        The address from which value is taken is calculated by the sum of sign-extended number which is specified
                        by bits [0:5] and the content stored in a register which is specified by bits [8:6] 
                            Example in assembly code:
                                LDR R0, R1, #1 ; R0 <- mem_read(R1 + 1)
                    */

                    uint16_t DR = (instr >> 9) & 0b111;  // destination register (DR) is define by bits [11:9]. 
                    uint16_t BaseR = (instr >> 6) & 0b111;  // BaseR is defined by bits [8:6]
                    uint16_t Offset6 = instr & 0b111111;  //  Offset6 is defined by the rightmost 6 bits of the instruction.

                    reg[DR] = mem_read(reg[BaseR] + sign_extend(Offset6, 6));
                    update_flags(DR);
                }
                break;
            case OP_LEA:
                {   
                    /*
                        Load effective address: Load an address to a register. The address that will be loaded is equal to the sum of
                        the incremented PC and the sign-extended number which is specified by bits [8:0] of the instruction.   
                            Example in assembly code:
                                LEA R0, LOOP ; R0 <- address of LOOP             
                    */

                    uint16_t DR = (instr >> 9) & 0b111;  // Destination register is the defined by bits [11:9]
                    uint16_t PCoffset9 = instr & 0b111111111;  // PCoffset9 is defined by bits [8:0]

                    reg[DR] = reg[R_PC] + sign_extend(PCoffset9, 9);
                    update_flags(DR);
                }
                break;
            case OP_ST:
                {   
                    /*
                        Store: store content of the register SR defined by bits [11:9] to a memory location.
                        The location is the sum of the incremented PC and the sign-extended number that specified by the last 9 bits (PCoffset9) of the instruction.
                            Example in assembly code:
                                ST R0, LOOP ; mem_write(LOOP, R0)
                    */

                    uint16_t SR = (instr >> 9) & 0b111;  //SR is defined by bits [11:9]
                    uint16_t PCoffset9 = instr & 0b111111111;  //PCoffset is specified by bits [8:0]

                    mem_write(reg[R_PC] + sign_extend(PCoffset9, 9), reg[SR]); 
                }
                break;
            case OP_STI:
                {
                    /*
                        Store indirect: store content of the register SR defined by bits [11:9] to a memory location defined in bits [8:0].  
                            Example in assembly code:
                                STI R0, LOOP ; mem_write(mem_read(LOOP), R0)
                    */
                    uint16_t SR = (instr >> 9) & 0b111;
                    uint16_t PCoffset9 = instr & 0b111111111;

                    mem_write(mem_read(reg[R_PC] + sign_extend(PCoffset9, 9)), reg[SR]);
                }
                break;
            case OP_STR:
                {
                    /*
                        Store register: store content of the register SR defined by bits [11:9] to a memory location.
                            Example in assembly code:
                                STR R0, R1, #1 ; mem_write(R1 + 1, R0)
                    */
                    uint16_t SR = (instr >> 9) & 0b111;
                    uint16_t BaseR = (instr >> 6) & 0b111;
                    uint16_t Offset6 = instr & 0b111111;
                    
                    mem_write(reg[BaseR] + sign_extend(Offset6, 6), reg[SR]);
                }
                break;
            case OP_TRAP:
                /*
                    Trap: Store the value of PC in register R_R7, then execute the instruction corresponding to travect8, which specify by the rightmost 8 bits
                    To display a signle character or string, we use putc() ot output each character to the standard output and then flush the output stream using fflush(stdout).
                    The reason why we use this method is that putc() and fflush() provide more control and efficiency for simple character output compared to the more feature-rich printf(). 
                */
                reg[R_R7] = reg[R_PC]; 
                uint16_t trapvect8 = instr & 0b11111111; // trapvect8 is specified by the bits [7:0]

                switch (trapvect8)
                {
                    case TRAP_GETC:
                        /*
                            Read a single character from the keyboard. The ASCII code of that character will be stored in register R_R0.
                        */
                        reg[R_R0] = (uint16_t)getchar();
                        update_flags(R_R0);
                        break;
                    case TRAP_OUT:
                        /*
                            Display the character that is currently stored in R_R0.
                        */
                        putc((char)reg[R_R0], stdout);
                        fflush(stdout);
                        break;
                    case TRAP_PUTS:
                        {
                            /*
                                Display a string (one by one character) onto the console monitor. The characters of string with be stored in consecutive locations in memory, the location 
                                of the first character is defined by value in register R_RO. TRAP_PUTS will terminate when it encounter x0000 in memory. 
                            */
                            uint16_t* c = memory + reg[R_R0];
                            while (*c)
                            {
                                putc((char)*c, stdout);
                                ++c;
                            }
                            fflush(stdout);
                        }
                        break;
                    case TRAP_IN:
                        {   
                            /*
                               Require user to enter a character from the keyboard. This character will be echoed onto the console display and stored in register R_R0 at the same time.
                            */
                            printf("Enter a character: ");
                            char c = getchar();
                            putc(c, stdout);
                            fflush(stdout);  // echo the entered character onto the console monitor.
                            reg[R_R0] = (uint16_t)c;  // strore the value in R_R0.
                            update_flags(R_R0);
                        }
                        break;
                    case TRAP_PUTSP:
                        {
                            /* 
                                Write a string onto the console monitor but in this case two characters are stored in each memory location (similarly to TRAP_PUTS, the characters are also 
                                stored in consecutive memory locations). The character which is specified by the rightmost 8 bits ([7:0]) will be read first and then the character defined
                                by the bits [15:8] is displayed. TRAP_PUTSP terminates when it encounters x0000 in the memory.
                            */
                            uint16_t* c = memory + reg[R_R0];
                            while (*c)
                            {
                                char char1 = (*c) & 0xFF;
                                putc(char1, stdout);
                                char char2 = (*c) >> 8;
                                if (char2) putc(char2, stdout);
                                ++c;
                            }
                            fflush(stdout);
                        }
                        break;
                    case TRAP_HALT:
                        /* 
                            Halt the execution and display the message onto console monitor.
                        */
                        puts("HALT");
                        fflush(stdout);
                        running = 0;
                        break;
                }
                break;
            case OP_RES:
            case OP_RTI:
            default:
                abort(); // Unimplemented instruction
                break;
        }
    }

    // When the program is interrupted, the terminal settings is restored back to normal.
    restore_input_buffering();
}
