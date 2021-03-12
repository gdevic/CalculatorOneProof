/*  Copyright (C) 2021  Goran Devic

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
*/
#include "Common.h"

// Addition/Subtraction heuristic:
// - If any one of the two terms is zero, return the other term, done.
// - Identify which term has the smaller exponent
// - Shift right its mantissa by the exponent difference
// - If the shift is larger than the buffer, return the larger value, done.
// - Based on the operation and the sign matrix, identify pure addition cases; add mantissa, normalize, done.
// - Find which mantissa is smaller
// - Swap so that we always subtract smaller value from the larger
// - Subtract mantissa with borrow, check for zero result, normalize, done.

TREG add_sub(TREG x, TREG y, bool is_sub)
{
    TREG result(is_sub ? (x.fp - y.fp) : (x.fp + y.fp));

    TASR scratch1(x); // scratch1 == Augend == x
    TASR scratch2(y); // scratch2 == Addend == y
    TASR scratch3; // result
    scratch_clear(scratch3);

    bool x_is_0 = scratch_is_0(scratch1);
    bool y_is_0 = scratch_is_0(scratch2);

    // This one needs to go first to capture the (x==0 && y==0) case
    if (y_is_0)
    {
        // Return the x value
        memcpy(result.mant, scratch1.mant, MAX_MANT);
        result.sign = x.sign;
        result.exps = x.exps;

        if (x_is_0) // Make it a true 0 (not potentially a negative zero)
        {
            result.exps = 128; // Make the result true 0
            result.sign = false;
        }
        return result;
    }
    if (x_is_0)
    {
        // Return the y value
        memcpy(result.mant, scratch2.mant, MAX_MANT);
        result.sign = y.sign ^ is_sub; // Notice the ^ is_sub !
        result.exps = y.exps;

        return result;
    }

    uint8_t exp_x = x.exps;
    uint8_t exp_y = y.exps;

    // If the required alignment shift is larger than the mantissa width, return the larger value unchanged
    if (exp_x < exp_y) // Shift right mantissa x
    {
        uint8_t shift = exp_y - exp_x;
        if (shift >= MAX_MANT)
        {
            // Return the y value
            memcpy(result.mant, scratch2.mant, MAX_MANT);
            result.sign = y.sign ^ is_sub; // Notice the ^ is_sub !
            result.exps = y.exps;

            return result;
        }
        while (shift--)
            scratch_shr(scratch1);

        result.exps = exp_y; // Result exponent is that of the 'y' term
    }
    else if (exp_x >= exp_y) // Shift right mantissa y
    {
        uint8_t shift = exp_x - exp_y;
        if (shift >= MAX_MANT)
        {
            // Return the x value
            memcpy(result.mant, scratch1.mant, MAX_MANT);
            result.sign = x.sign;
            result.exps = x.exps;

            return result;
        }
        while (shift--)
            scratch_shr(scratch2);

        result.exps = exp_x; // Result exponent is that of the 'x' term
    }
    else
        result.exps = exp_x; // All values have the same exponent

    // Based on the operation and signs of each term, calculate the *effective* operation (add or sub)
    bool x_neg = x.sign;
    bool y_neg = y.sign;
    bool is_addition = (!is_sub && ((!x_neg && !y_neg) || (x_neg && y_neg))) || (is_sub && ((x_neg && !y_neg) || (!x_neg && y_neg)));

    if (is_addition)
    {
        // ----------- ADDITION OPERATION -----------
        // Add individual mantissa BCD digits, with carry to overflow
        bool carry = 0;
        for (int k = MAX_MANT - 1; k >= 0; k--)
        {
            char bcd1 = scratch1.mant[k] - '0';
            char bcd2 = scratch2.mant[k] - '0';
            char sum = bcd_adc(bcd1, bcd2, carry);
            scratch3.mant[k] = sum + '0';
        }

        // If we have a carry set after the MSB digit, we need to insert "1" as the topmost digit
        if (carry)
        {
            scratch_shr(scratch3);
            scratch3.mant[0] = '1';
            result.exps++; // Also adjust the exponent
        }

        // The sign of the result is the sign of any one of the terms (since they are both the same)
        result.sign = x.sign;
    }
    else
    {
        // ----------- SUBTRACTION OPERATION -----------
        bool x_ge_y = scratch_is_greater_or_equal(scratch1, scratch2);
        if (!x_ge_y) // Subtract smaller from the larger value: scratch1 - scratch2
            scratch_swap(scratch1, scratch2);

        // Subtract individual mantissa BCD digits, with borrow
        // The borrow will never undeflow the final value since we are always subtracting a smaller mantissa from the larger one
        bool borrow = 0;
        for (int k = MAX_MANT - 1; k >= 0; k--)
        {
            // Subtract the smaller mantissa from the larger
            char bcd1 = scratch1.mant[k] - '0';
            char bcd2 = scratch2.mant[k] - '0';
            char sub = bcd_sbc(bcd1, bcd2, borrow);
            scratch3.mant[k] = sub + '0';
        }
        if (borrow)
            std::cerr << "Unexpected borrow in " << __FUNCTION__ << ":" << __LINE__ << "\n";

        // The sign of the result is the sign of the first term XOR whether we swapped the numbers
        // when we subtracted the smaller value from the larger
        result.sign = x.sign ^ !x_ge_y;

        if (scratch_is_0(scratch3))
        {
            result.exps = 128; // Make the result true 0
            result.sign = false;
        }
        else // Normalize the result
        {
            while (scratch3.mant[0] == '0')
            {
                scratch_shl(scratch3);
                result.exps--; // Also adjust the exponent
            }
        }
    }

    memcpy(result.mant, scratch3.mant, MAX_MANT);

    return result;
}

TREG add_sub(const char *a, const char *b, bool is_sub)
{
    return add_sub(input(a), input(b), is_sub);
}

void add_sub_test()
{
    std::cout << "ADDITION / SUBTRACTION TEST\n";
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

    static const std::string op[2] = {"Addition", "Subtraction"};
    static const std::string header[4] = {
        " of non-exponential numbers:",
        " of non-exponential negative with positive number -x,y:",
        " of non-exponential positive with negative number x,-y:",
        " of non-exponential negative with negative number -x,-y:"
    };

#if 1
    // Run two operations using our set of test numbers
    for (int test_number = 1, addition = 0; addition < 2; addition++)
    {
        // Loop for all sign variations: bit 0, bit 1 are x, y mantissa signs
        for (int signs = 0; signs < 4; signs++)
        {
            std::cout << op[addition] << header[signs] << "\n";
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
                    std::cout << s2 << (addition ? " - " : " + ") << t2;
                    add_sub(s2.c_str(), t2.c_str(), addition).print(test_number++);
                }
            }
        }
    }
#endif
#if 1
    // Pseudo-random exponential tests: we pick from the list of Non-exponential numbers
    // and, modify their first few digits, randomize their signs and exponent (within some limits)
    std::cout << "ADDITION / SUBTRACTION RANDOMIZED TESTS\n";
    std::cout << h1;
    rnd.seed(43); // Reproducible random number seed
    for (int test_number = 1; test_number <= 500; test_number++)
    {
        int index1 = rnd() % tests.size();
        int index2 = rnd() % tests.size();
        int op = rnd() % 2; // Addition, subtraction

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

        std::cout << s1 << (op ? " - " : " + ") << s2;
        add_sub(s1.c_str(), s2.c_str(), op).print(test_number);
    }
#endif
}
