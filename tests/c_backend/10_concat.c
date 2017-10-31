#include <lpg_std_unit.h>
#include <lpg_std_string.h>
#include <stdio.h>
static string_ref lambda_1(void);
static string_ref lambda_1(void)
{
    /*side effect*/
    return string_literal("b", 1);
}
int main(void)
{
    string_ref const r_6 = lambda_1();
    string_ref const r_7 = string_ref_concat(string_literal("a", 1), r_6);
    unit const r_8 = unit_impl;
    fwrite(r_7.data, 1, r_7.length, stdout);
    string_ref_free(&r_6);
    string_ref_free(&r_7);
    return 0;
}
