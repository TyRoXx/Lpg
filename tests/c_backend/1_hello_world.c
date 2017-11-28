#include <lpg_std_unit.h>
#include <lpg_std_string.h>
#include <stdio.h>
int main(void)
{
    string_ref const r_2 = string_literal("Hello, world!", 13);
    unit const r_3 = unit_impl;
    fwrite(r_2.data, 1, r_2.length, stdout);
    return 0;
}
