#include <lpg_std_unit.h>
#include <lpg_std_string.h>
#include <stdio.h>
#include <lpg_std_read.h>
static string_ref lambda_1(string_ref const arg0)
{
    return arg0;
}
int main(void)
{
    string_ref const r_5 = read_impl();
    string_ref const r_6 = lambda_1(r_5);
    unit const r_7 = unit_impl;
    fwrite(r_6.data, 1, r_6.length, stdout);
    string_ref_free(&r_5);
    string_ref_free(&r_6);
    return 0;
}
