/*  Copyright (C) 2021  Goran Devic

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
*/
#include "TReg.h"

void input_test();
void add_sub_test();
void mult_test();
void div_test();

uint32_t tests_total = 0;
uint32_t tests_pass = 0;
uint32_t tests_fail = 0;

int main()
{
    input_test();
    add_sub_test();
    mult_test();
    div_test();

    std::cout << "Total tests: " << tests_total << "  fail: " << tests_fail << "  rounding errors: " << (tests_total - (tests_pass + tests_fail)) << "\n";
}
