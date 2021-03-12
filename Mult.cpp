/*  Copyright (C) 2021  Goran Devic

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
*/
#include "Common.h"

// Multiplication heuristic:
// - If any one of the two terms is zero, return zero, done.
// - The sign of the result is the xor of the signs of individual terms
// - If any one of the terms is zero, return zero, done.
// - The exponent of the result is the sum of the exponents of individual terms
// - Multiply each digit of multiplicand with each digit of multiplier and keep summing each product
// - Normalize the result

TREG mult(TREG x, TREG y)
{
    TREG result(x.fp * y.fp);

    TASR scratch1(x); // scratch1 == Multiplicand == x
    TASR scratch2(y); // scratch2 == Multiplier == y
    TASR scratch3; // result
    scratch_clear(scratch3);
    TASR scratch4; // Temp scratch

    // The sign of the result is the xor of the signs of individual terms
    result.sign = x.sign ^ y.sign;

    bool x_is_0 = scratch_is_0(scratch1);
    bool y_is_0 = scratch_is_0(scratch2);

    if (x_is_0 || y_is_0)
        return result; // Return zero

    // The exponent of the result is the sum of the exponents of individual terms
    result.exps = exp_add(x, y);
    // XXX Process overflows

    // ----------- MULTIPLICATION OPERATION -----------
    // Multiply individual mantissa BCD digits, then add and shift the running total result
    for (int8_t j = MAX_MANT - 1; j >= 0; j--) // Index of y.mant
    {
        scratch_shr(scratch3);

        for (int8_t i = MAX_MANT - 1; i >= 0; i--) // Index of x.mant
        {
            char bcd1 = scratch1.mant[i] - '0';
            char bcd2 = scratch2.mant[j] - '0';
            char product = bcd_mult(bcd1, bcd2);
            char nibble0 = product & 0xF;
            char nibble1 = (product >> 4) & 0xF;

            scratch_clear(scratch4);

            scratch4.mant[i + 1] = nibble0 + '0';
            scratch4.mant[i - 1 + 1] = nibble1 + '0';

            // Add temp arith register to the final result register
            // Add individual mantissa BCD digits, with carry to overflow
            bool carry = 0;
            for (int k = MAX_SCRATCH - 1; k >= 0; k--)
            {
                char bcd1 = scratch3.mant[k] - '0';
                char bcd2 = scratch4.mant[k] - '0';
                char sum = bcd_adc(bcd1, bcd2, carry);
                scratch3.mant[k] = sum + '0';
            }
            if (carry)
                std::cerr << "Unexpected carry in " << __FUNCTION__ << ":" << __LINE__ << "\n";
        }
    }

    // Normalize the result in the scratch register
    if (scratch3.mant[0] == '0')
        scratch_shl(scratch3);
    else
        result.exps++;

    memcpy(result.mant, scratch3.mant, MAX_MANT);

    return result;
}

TREG mult(const char *a, const char *b)
{
    return mult(input(a), input(b));
}

void mult_test()
{
    std::cout << "MULTIPLICATION TEST\n";
    const std::string h1 = " Operand 1       OP Operand 2         Internal normalized    Exp    ID  Internal printed          Verification value\n";

    // Input buffer: 16 characters
    //   0123456789012345
    static std::vector<std::string> tests = { // Non-exponential numbers
        " 1              ",
        " 1.000000000001 ",
        " 1.0000000000001",
        " 1.2345678901234",
        " 1234567890123.4",
        " 123456789012345",
        " 9              ",
        " 99             ",
        " 99999999999999 ",
        " 999999999999999",
        " 0              ",
        " 0.1            ",
        " 0.01           ",
        " 0.0000000000001",
        " 0.0000000000009",
        " 0.1234567890123",
        " 3.1415926535897",
        " 2.7182818284590",
    };

    static const std::string header[4] = {
        " of non-exponential numbers:",
        " of non-exponential negative with positive number -x,y:",
        " of non-exponential positive with negative number x,-y:",
        " of non-exponential negative with negative number -x,-y:"
    };

#if 0
    // Test bcd_mult()
    static char result[3];
    // Loop for all single-digit BCD numbers in x and y space, and with and without carry variations
    for (int carry = 0; carry < 2; carry++)
    {
        for (int x = 0; x < 10; x++)
        {
            for (int y = 0; y < 10; y++)
            {
                sprintf(result, "%02d", x * y + carry);
                unsigned char product = bcd_mult(x, y, carry);
                if ((((product >> 4) + '0') != result[0]) || (((product & 0xF) + '0') != result[1]))
                    std::cout << "Error with " << x << " * " << y << " +" << carry << " = " << result << "\n";
            }
        }
    }
    // Test the results with carry-in
    std::cout << "Finished testing bcd_mult()\n";
#endif

#if 1
    // Run the operation using our set of test numbers
    // Loop for all sign variations: bit 0, bit 1 are x, y mantissa signs
    for (int test_number = 1, signs = 0; signs < 4; signs++)
    {
        std::cout << "Multiplication" << header[signs] << "\n";
        std::cout << h1;
        // Combine each number from the test set with each other
        for (std::string &s : tests)
        {
            for (std::string &t : tests)
            {
                std::string s2 = s;
                if (signs & 1)
                    s2[0] = '-';
                std::string t2 = t;
                if (signs & 2)
                    t2[0] = '-';
                std::cout << s2 << " * " << t2;
                mult(s2.c_str(), t2.c_str()).print(test_number++);
            }
        }
    }
#endif
#if 1
    // Pseudo-random exponential tests: we pick from the list of Non-exponential numbers
    // and, modify their first few digits, randomize their signs and exponent (within some limits)
    std::cout << "MULTIPLICATION RANDOMIZED TESTS\n";
    std::cout << h1;
    rnd.seed(43); // Reproducible random number seed
    for (int test_number = 1; test_number <= 500; test_number++)
    {
        int index1 = rnd() % tests.size();
        int index2 = rnd() % tests.size();

        std::string s1 = tests[index1].substr(0, 12);
        s1[1] = rdigit(10);
        if (s1[2] == ' ')
            s1[2] = '.';
        s1[3] = rdigit(10);
        s1[0] = (rnd() & 1) ? ' ' : '-';
        // Needs to be in a separate line for rnd() consistency across the platforms
        char e1 = rdigit(2), e2 = rdigit(10);
        s1 = s1 + "E" + ((rnd() & 1) ? '-' : '+') + e1 + e2;

        std::string s2 = tests[index2].substr(0, 12);
        s2[1] = rdigit(10);
        if (s2[2] == ' ')
            s2[2] = '.';
        s2[3] = rdigit(10);
        s2[0] = (rnd() & 1) ? ' ' : '-';
        e1 = rdigit(2), e2 = rdigit(10);
        s2 = s2 + "E" + ((rnd() & 1) ? '-' : '+') + e1 + e2;

        std::cout << s1 << " * " << s2;
        mult(s1.c_str(), s2.c_str()).print(test_number);
    }
#endif
}
