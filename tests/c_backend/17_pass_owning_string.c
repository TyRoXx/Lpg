#include <lpg_std_unit.h>
#include <lpg_std_string.h>
#include <stdio.h>
static string_ref lambda_1(void);
static unit lambda_2(string_ref const a_0);
static string_ref lambda_1(void)
{
    /*side effect*/
    return string_literal("b", 1);
}
static unit lambda_2(string_ref const a_0)
{
    unit const r_3 = unit_impl;
    fwrite(a_0.data, 1, a_0.length, stdout);
    return r_3;
}
int main(void)
{
    string_ref const r_4 = lambda_1();
    string_ref const r_6 = string_ref_concat(r_4, string_literal("c", 1));
    unit const r_7 = lambda_2(r_6);
    string_ref_free(&r_4);
    string_ref_free(&r_6);
    return 0;
}
