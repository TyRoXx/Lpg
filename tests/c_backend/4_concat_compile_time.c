#include <lpg_std_unit.h>
#include <lpg_std_string.h>
#include <stdio.h>
int main(void)
{
    string_ref const s_0 = string_literal("123456", 6);
    unit const r_3 = unit_impl;
    fwrite(s_0.data, 1, s_0.length, stdout);
    return 0;
}
