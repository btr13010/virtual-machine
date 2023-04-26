#include <stdio.h>
#include <stdint.h>
# include <stdlib.h>

/* For Unix */
// #include <unistd.h>
// #include <fcntl.h>
// #include <sys/time.h>
// #include <sys/types.h>
// #include <sys/termios.h>
// #include <sys/mman.h>

// // Input buffer size
// struct termios original_tio;

// void disable_input_buffering() {
//     /*
//         This function disables the input buffering.
//         Input buffering is a mechanism that allows the operating system to hold the input until the user presses the enter key.
//         This function is used to disable the input buffering so that the program can read the input immediately.
//     */

//     tcgetattr(STDIN_FILENO, &original_tio);
//     struct termios new_tio = original_tio;
//     new_tio.c_lflag &= ~ICANON & ~ECHO;
//     tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);
// }

// void restore_input_buffering() {
//     /*
//         This function restores the input buffering.
//         This function is used to restore the input buffering after the program has finished reading the input.
//     */

//     tcsetattr(STDIN_FILENO, TCSANOW, &original_tio);
// }

// uint16_t check_key() {
//     /*
//         This function checks if the user has pressed a key.
//         If the user has pressed a key, the function returns 1, otherwise it returns 0.
//     */

//     fd_set readfds;
//     FD_ZERO(&readfds);
//     FD_SET(STDIN_FILENO, &readfds);

//     struct timeval timeout;
//     timeout.tv_sec = 0;
//     timeout.tv_usec = 0;
//     return select(1, &readfds, NULL, NULL, &timeout) != 0;
// }

/* For Windows */
#include <Windows.h>
#include <conio.h>
HANDLE hStdin = INVALID_HANDLE_VALUE;
DWORD fdwMode, fdwOldMode;

void disable_input_buffering()
{
    hStdin = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(hStdin, &fdwOldMode); /* save old mode */
    fdwMode = fdwOldMode
            ^ ENABLE_ECHO_INPUT  /* no input echo */
            ^ ENABLE_LINE_INPUT; /* return when one or
                                    more characters are available */
    SetConsoleMode(hStdin, fdwMode); /* set new mode */
    FlushConsoleInputBuffer(hStdin); /* clear buffer */
}

void restore_input_buffering()
{
    SetConsoleMode(hStdin, fdwOldMode);
}

uint16_t check_key()
{
    return WaitForSingleObject(hStdin, 1000) == WAIT_OBJECT_0 && _kbhit();
}

uint16_t swap16(uint16_t x) {
    /*
        The LC-3 is a little-endian architecture, but most modern computers are big-endian.
        In little-endian, the first byte is the least significant digit, and in big-endian, it is reversed.
        In order to swap a value from big-endian to little-endian, we need to reverse the order of the bytes.

        This function swaps the bytes of a 16-bit value to convert between big-endian and little-endian.
    */

    return (x << 8) | (x >> 8); // shift the first byte to the right by 8 bits and the second byte to the left by 8 bits
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

uint16_t sign_extend(uint16_t x, int bit_count) {
    /*
        This function sign extends a value to 16 bits.
        Sign extension is used to convert a value from a smaller data type to a larger data type while preserving the value's sign.
        For example, if we have a 5-bit value 0b11111, we can sign extend it to 16 bits by adding 11 0s to the left of the value.
        This will give us 0b1111111111111111 which is -1 in decimal.

        Input: 
            x: the value to be sign extended
            bit_count: the number of bits in the value
    */

    if ((x >> (bit_count - 1)) & 1) {
        x |= (0xFFFF << bit_count); // if the most significant bit is 1, we add 1s to the left of the value
    }
    return x;
}