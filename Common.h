#include "TReg.h"
#include <random>
#include <vector>

TREG input(const char *in);

extern std::minstd_rand rnd;
char rdigit(int n);

// Candidates for CPU instructions:

// Single digit BCD adder with carry
char bcd_adc(char bcd1, char bcd2, bool &carry);

// Single digit BCD subtract with borrow
char bcd_sbc(char bcd1, char bcd2, bool &borrow);

// Single digit BCD multiply (result uses two nibbles and is packed)
char bcd_mult(char bcd1, char bcd2);

// Add/subtract two exponents, setting the overflow flag if needed (TODO)
uint8_t exp_add(TREG &x, TREG &y);
uint8_t exp_sub(TREG &x, TREG &y);

// Return true if scratch buffer 1 >= buffer 2
bool scratch_is_greater_or_equal(TASR scratch1, TASR scratch2);

// Swap the contents of two scratch registers
void scratch_swap(TASR &scratch1, TASR &scratch2);

// Shift scratch buffer one digit to the right/left, filling in '0'
void scratch_shr(TASR &scratch);
void scratch_shl(TASR &scratch);

// Return true is the scratch register is zero
bool scratch_is_0(TASR scratch);

// Clear the scratch register
void scratch_clear(TASR &scratch);
