/*  Copyright (C) 2021  Goran Devic

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
*/
#include "Common.h"

// Dividend: a or x
// ---------------- = Quotient: result
// Divisor:  b or y
//
// "Binary Coded Decimal (BCD) Division by Shift and Subtract"
// Division heuristic:
// - If the divisor is zero, signal error, done.
// - If the dividend is zero, return zero, done.
// - The sign of the result is the xor of the signs of individual terms
// - The exponent of the result is the difference of the exponents of individual terms
// - While dividend >= divisor, subtract divisor and increment quotient digit
// - Otherwise, shift dividend left by one digit and repeat until all digits are processed
// - Normalize the result

TREG div(TREG x, TREG y)
{
    TREG result(x.fp / y.fp);

    TASR scratch1(x); // scratch1 == Dividend == x
    TASR scratch2(y); // scratch2 == Divisor == y
    TASR scratch3; // result
    scratch_clear(scratch3);

    // The sign of the result is the xor of the signs of the individual terms
    result.sign = x.sign ^ y.sign;

    bool x_is_0 = scratch_is_0(scratch1);
    bool y_is_0 = scratch_is_0(scratch2);

    if (y_is_0)
    {
        // XXX "Division by zero error"
        std::cout << " *** DIV0 *** ";
        result.exps = 0; // XXX Signal to DIV0 error
        return result; // Return zero
    }
    if (x_is_0)
        return result; // Return zero

    // The exponent of the result is the difference of the exponents of individual terms
    result.exps = exp_sub(x, y);
    // XXX Process overflows and underflows

    // Before we start, shift both dividend and divisor one digit to the right, freeing the most significant digit
    // This is done to compensate for the first dividend shift left in the cases when it was less than the divisor
    scratch_shr(scratch1);
    scratch_shr(scratch2);

    // ----------- DIVISION OPERATION -----------
    for (int8_t i = 0; i < MAX_SCRATCH; i++) // MSB to LSB processing
    {
        while (scratch_is_greater_or_equal(scratch1, scratch2)) // Divisor will go into a dividend
        {
            // Subtract divisor from a dividend and assign the result to be the new dividend

            // Subtract individual mantissa BCD digits, with borrow
            // The borrow will never undeflow the final value since we are always subtracting a smaller mantissa from the larger one
            bool borrow = 0;
            for (int k = MAX_SCRATCH - 1; k >= 0; k--)
            {
                char bcd1 = scratch1.mant[k] - '0';
                char bcd2 = scratch2.mant[k] - '0';
                char sub = bcd_sbc(bcd1, bcd2, borrow);
                scratch1.mant[k] = sub + '0';
            }
            if (borrow)
                std::cerr << "Unexpected borrow in " << __FUNCTION__ << ":" << __LINE__ << "\n";

            if (scratch3.mant[i] > '9')
                std::cerr << "Unexpected scratch mant digit " << scratch3.mant[i] << " i=" << int(i) << " " << __FUNCTION__ << ":" << __LINE__ << "\n";

            scratch3.mant[i]++; // Increment the quotient digit by one
        }

        // Shift left dividend by one digit and repeat until all digits are processed
        scratch_shl(scratch1);
    }

    // Normalize the result in the scratch register
    if (scratch3.mant[0] == '0')
    {
        scratch_shl(scratch3);
        result.exps--;
    }

    memcpy(result.mant, scratch3.mant, MAX_MANT);

    return result;
}

TREG div(const char *a, const char *b)
{
    return div(input(a), input(b));
}

void div_test()
{
    std::cout << "DIVISION TEST\n";
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

#if 1
    // Run the operation using our set of test numbers
    // Loop for all sign variations: bit 0, bit 1 are x, y mantissa signs
    for (int test_number = 1, signs = 0; signs < 4; signs++)
    {
        std::cout << "Division" << header[signs] << "\n";
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
                std::cout << s2 << " / " << t2;
                div(s2.c_str(), t2.c_str()).print(test_number++);
            }
        }
    }
#endif
#if 1
    // Pseudo-random exponential tests: we pick from the list of Non-exponential numbers
    // and, modify their first few digits, randomize their signs and exponent (within some limits)
    std::cout << "DIVISION RANDOMIZED TESTS\n";
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

        std::cout << s1 << " / " << s2;
        div(s1.c_str(), s2.c_str()).print(test_number);
    }
#endif
}
