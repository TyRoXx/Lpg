#include <lpg_std_unit.h>
#include <lpg_std_string.h>
#include <stdio.h>
static unit lambda_1(string_ref const a_0);
static unit lambda_1(string_ref const a_0)
{
    unit const r_3 = unit_impl;
    fwrite(a_0.data, 1, a_0.length, stdout);
    return r_3;
}
int main(void)
{
    unit const r_2 = lambda_1(string_literal("a", 1));
    return 0;
}
