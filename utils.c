#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/termios.h>

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

uint16_t swap16(uint16_t x) {
    /*
        The LC-3 is a little-endian architecture, but most modern computers are big-endian.
        In little-endian, the first byte is the least significant digit, and in big-endian, it is reversed.
        In order to swap a value from big-endian to little-endian, we need to reverse the order of the bytes.

        This function swaps the bytes of a 16-bit value to convert between big-endian and little-endian.
    */

    return (x << 8) | (x >> 8); // shift the first byte to the right by 8 bits and the second byte to the left by 8 bits
}