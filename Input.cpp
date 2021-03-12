/*  Copyright (C) 2021  Goran Devic

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
*/
#include "Common.h"

// Assumptions on the input source buffer (as typed in by the user):
// * The input buffer has to be exactly 16 chars wide
// * char[0] can only contain a mantissa sign ' ' (pos) or '-' (neg)
// * char[1..] represents the mantissa using digits '0'..'9' with a single (optional) decimal point '.'
// * 'E' terminates the mantissa and starts the exponent section
// * ' ' (spaces) can also terminate the mantissa
// * 'E' specifier is optional; absence of it defaults to "E+00"
// * 'E' needs to be located at the [12] position in the buffer
// * 'E' is followed by '+' or '-', the exponent sign
// * 'E'+2 represents the exponent '00' to '99', exactly 2 characters wide
// * This input buffer will be processed and checked for these rules by some intermediate process

TREG input(const char *in)
{
    TREG result(in);

    TASR scratch3; // result
    scratch_clear(scratch3);

    // 2 Basic formats: with exponent and without it
    uint8_t maxi = 16; // When not specifying the exponent, the final index of the mantissa character sequence
    if (in[12] == 'E') // With explicit exponent
    {
        char e0 = in[14] - '0';
        char e1 = in[15] - '0';
        int8_t e = e0 * 10 + e1; // It is simple to multiply by 10 by using shifts and adds
        if (in[13] == '-')
            e = 128 + (~e + 1); // 2's complement
        else
            e |= 0x80; // exponent bias
        result.exps = e;
        maxi = 12; // When specifying the exponent, the final index of the mantissa character sequence
    }
    else
        result.exps = 128; // Zero exponent (biased)

    // Mantissa sign
    result.sign = in[0] == '-'; // sign is negative

    // Parse the input buffer and create a normalized mantissa.
    // The location of decimal point will determine which direction do we adjust the exponent:
    // For numbers >= 1, we increment the exponent by the number of digits in the integer portion (to the left of decimal point)
    // For numbers < 1, we decrement the exponent by the number of zeroes following the decimal point
    uint8_t i = 1; // Index into the source string
    uint8_t j = 0; // Index into the normalized buffer
    int8_t adjust = -1; // Initial exponent adjustment value (always ignore the first digit)

    // Ignore leading zeroes in the source mantissa
    while ((in[i] == '0') && (i <= MAX_MANT)) i++; // XXX how to handle it when i reaches the end of the source buffer?

    if (in[i] == '.') // Number < 1
    {
        // Skip the decimal point
        if (i != maxi) i++;

        // Count leading zeroes
        while (in[i] == '0')
        {
            adjust--;
            i++;
        }
    }
    else
    {
        // Find the decimal point to know the exponent adjustment
        uint8_t i2 = i;
        while (isdigit(in[i2]) && (i2 != maxi))
        {
            adjust++;
            i2++;
        }
    }

    // Copy remaining digits of the mantissa, ignore the decimal point
    while ((isdigit(in[i]) || (in[i] == '.')) && (i != maxi) && (j < MAX_MANT))
    {
        if (in[i] != '.')
        {
            scratch3.mant[j] = in[i];
            j++;
        }
        i++;
    }

    // Adjust the exponent if the mantissa was non-zero
    if (j)
        result.exps += adjust;
    else
        result.exps = 128; // If the mantissa was zero, set the exponent to zero as well

    memcpy(result.mant, scratch3.mant, MAX_MANT);

    return result;
}

void input_test()
{
    std::cout << "INPUT PARSER TEST\n";
    const std::string h1 = " Input buffer      Internal normalized    Exp    ID  Internal printed          Verification value\n";
    int test_number = 1;

    // Input buffer: 16 characters
    //   0123456789012345
    static std::vector<std::string> tests1 = { // Non-exponential numbers
        " 1              ",
        " 1.             ",
        " 1.0            ",
        " 1.00           ",
        " 1.000000000000 ",
        " 1.000000000001 ",
        " 1.0000000000001",
        " 1.0000000000000",
        " 1.2345678901234",
        " 12.345678901234",
        " 1234567890123.4",
        " 12345678901234.",
        " 123456789012345",
        " 999999999999999",
        " 000000000000000",
        " 000000000000001",
        " 0              ",
        " 0.             ",
        " 0.0            ",
        " 0.0000000000000",
        " 0.1            ",
        " 0.01           ",
        " 0.0000000000001",
        " 0.1234567890123",
        " 0.9999999999999",
    };

    // Test both positive and negative variations of the input values
    std::cout << "Non-exponential numbers:\n";
    std::cout << h1;
    for (std::string &s : tests1)
        input(s.c_str()).print(test_number++);

    std::cout << "Non-exponential negative numbers:\n";
    std::cout << h1;
    for (std::string &s : tests1)
    {
        std::string a = s;
        a[0] = '-';
        input(a.c_str()).print(test_number++);
    }

    // Input buffer: 16 characters
    //   0123456789012345
    static std::vector<std::string> tests2 = { // Numbers with explicit exponents
        " 1          E+12",
        " 1.         E+45",
        " 1.0        E+00",
        " 1.00       E+99",
        " 1.000000000E+12",
        " 1.000000000E+00",
        " 1.234567890E+65",
        " 12.34567890E+54",
        " 12345678901E+43",
        " 99999999999E+32",
        " 0          E+23",
        " 0.         E+67",
        " 0.0        E+99",
        " 0.000000000E+00",
        " 0.1        E+23",
        " 0.01       E+67",
        " 0.000000000E+54",
        " 0.123456789E+22",
        " 0.999999999E+01",
        " 0.123456789E+01",
        " 0.999999999E+02",
        " 12.34567890E+34",
        " 12345678901E+85",
        " 99999999999E+99", // XXX Normalizing this value should result in overflow/underflow which we will ignore for now
    };

    static const std::string header[4] = {
        "Numbers with explicit exponents:",
        "Negative numbers with explicit exponents:",
        "Numbers with explicit negative exponents:",
        "Negative numbers with explicit negative exponents:"
    };

    // Loop for all signs variations: bit 0, bit 1 are mantissa, exponent signs
    for (int signs = 0; signs < 4; signs++)
    {
        std::cout << header[signs] << "\n";
        std::cout << h1;
        for (std::string &s : tests2)
        {
            std::string a = s;
            if (signs & 1)
                a[0] = '-';
            if (signs & 2)
                a[13] = '-';
            input(a.c_str()).print(test_number++);
        }
    }
}
