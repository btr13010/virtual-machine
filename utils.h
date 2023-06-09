#ifndef UTILS_H
#define UTILS_H

void disable_input_buffering();
void restore_input_buffering();
uint16_t check_key();
uint16_t swap16(uint16_t x);
void handle_interrupt(int signal);
uint16_t sign_extend(uint16_t x, int bit_count);

#endif