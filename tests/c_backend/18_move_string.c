#include <lpg_std_assert.h>
#include <lpg_std_string.h>
#include <stdio.h>
#include <lpg_std_read.h>
static string_ref lambda_1(string_ref const arg0);
static string_ref lambda_1(string_ref const arg0)
{
    string_ref_add_reference(&arg0);
    return arg0;
}
int main(void)
{
    string_ref const r_5 = read_impl();
    string_ref const r_7 = string_ref_concat(r_5, string_literal("", 0));
    string_ref const r_8 = lambda_1(r_7);
    bool const r_14 = string_ref_equals(string_literal("", 0), r_8);
    unit const r_15 = assert_impl(r_14);
    string_ref_free(&r_5);
    string_ref_free(&r_7);
    string_ref_free(&r_8);
    return 0;
}
