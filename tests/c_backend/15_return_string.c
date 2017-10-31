#include <lpg_std_unit.h>
#include <lpg_std_string.h>
#include <stdio.h>
static string_ref lambda_1(void);
static string_ref lambda_1(void)
{
    /*side effect*/
    unit const r_2 = unit_impl;
    return string_literal("abc", 3);
}
int main(void)
{
    string_ref const r_3 = lambda_1();
    unit const r_4 = unit_impl;
    fwrite(r_3.data, 1, r_3.length, stdout);
    string_ref_free(&r_3);
    return 0;
}
