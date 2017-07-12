#include <lpg_std_unit.h>
#include <lpg_std_string.h>
#include <stdio.h>
static unit lambda_1(string_ref const arg0)
{
    unit const r_3 = unit_impl;
    fwrite(arg0.data, 1, arg0.length, stdout);
    return r_3;
}
int main(void)
{
    unit const r_2 = lambda_1(string_ref_create("a", 1));
    return 0;
}
