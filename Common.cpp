/*  Copyright (C) 2021  Goran Devic

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
*/
#include "Common.h"

// Random number generator that produces equivalent sequence of values across various platforms
std::minstd_rand rnd;
char rdigit(int n) { return (rnd() % n) + '0'; }

// Candidates for CPU instructions:

// Single digit BCD adder with carry
char bcd_adc(char bcd1, char bcd2, bool &carry)
{
    char sum = bcd1 + bcd2 + carry;
    carry = sum > 9;
    if (carry)
        sum = (sum + 6) & 0xF; // "DAA" - decimal adjust after addition
    return sum;
}

// Single digit BCD subtract with borrow
char bcd_sbc(char bcd1, char bcd2, bool &borrow)
{
    int sub = bcd1 - bcd2 - borrow;
    borrow = sub < 0;
    if (borrow)
        sub = (sub + 10) & 0xF; // "DAS" - decimal adjust after subtraction
    return sub;
}

// Single digit BCD multiply (result uses two nibbles and is packed)
char bcd_mult(char bcd1, char bcd2)
{
    // Multiply 2 BCD digits (add carry) into an 8-bit wide binary result
#if 0
    uint8_t product = bcd1 * bcd2;
#else
    uint8_t product = 0;
    for (int i = 0; i < 4; i++)
    {
        if (bcd2 & 1)
            product += bcd1;
        bcd2 >>= 1;
        bcd1 <<= 1;
    }
#endif
    // Convert from 8-bit wide binary to 2 BCD digits
    // Double-Dabble Binary-to-BCD Algorithm: https://en.wikipedia.org/wiki/Double_dabble
    uint16_t final = product;
    for (int i = 0; i < 8; i++)
    {
        if (((final >> 8) & 0xF) >= 5)
            final = ((final + 0x0300) & 0x0F00) | (final & 0xF0FF);
        if (((final >> 12) & 0xF) >= 5)
            final = ((final + 0x3000) & 0xF000) | (final & 0x0FFF);
        final <<= 1;
    }
    return char(final >> 8);
}

// Add two exponents, setting the overflow flag if needed
uint8_t exp_add(TREG &x, TREG &y)
{
    // XXX Handle overflow and underflow
    int sum = (int(x.exps) - 128) + (int(y.exps) - 128) + 128;
    return uint8_t(sum);
}

// Subtract two exponents, setting the overflow flag if needed
uint8_t exp_sub(TREG &x, TREG &y)
{
    // XXX Handle overflow and underflow
    int sum = (int(x.exps) - 128) - (int(y.exps) - 128) + 128;
    return uint8_t(sum);
}

// Return true if scratch buffer 1 >= buffer 2
bool scratch_is_greater_or_equal(TASR scratch1, TASR scratch2)
{
    for (uint8_t i = 0; i < MAX_SCRATCH; i++)
    {
        if (scratch1.mant[i] > scratch2.mant[i])
            return true;
        if (scratch1.mant[i] < scratch2.mant[i])
            return false;
    }
    return true;
}

// Swap the contents of two scratch registers
void scratch_swap(TASR &scratch1, TASR &scratch2)
{
    for (uint8_t i = 0; i < MAX_SCRATCH; i++)
    {
        scratch1.mant[i] ^= scratch2.mant[i];
        scratch2.mant[i] ^= scratch1.mant[i];
        scratch1.mant[i] ^= scratch2.mant[i];
    }
}

// Shift shratch buffer one digit to the right, filling in '0'
void scratch_shr(TASR &scratch)
{
    std::memmove(&scratch.mant[1], &scratch.mant[0], MAX_SCRATCH - 1); // SHR
    scratch.mant[0] = '0';
}

// Shift shratch buffer one digit to the left, filling in '0'
void scratch_shl(TASR &scratch)
{
    std::memmove(&scratch.mant[0], &scratch.mant[1], MAX_SCRATCH - 1); // SHL
    scratch.mant[MAX_SCRATCH - 1] = '0';
}

// Return true is the scratch register is zero
bool scratch_is_0(TASR scratch)
{
    for (uint8_t i = 0; i < MAX_SCRATCH; i++)
    {
        if (scratch.mant[i] != '0')
            return false;
    }
    return true;
}

// Clear the scratch register
void scratch_clear(TASR &scratch)
{
    std::memset(scratch.mant, '0', MAX_SCRATCH);
}
