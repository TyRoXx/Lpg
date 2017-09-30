#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <lpg_std_string.h>
#include <stdio.h>
typedef string_ref (*type_definition_0)(string_ref);
static type_definition_0 lambda_1(void);
static string_ref lambda_2(string_ref const arg0);
static type_definition_0 lambda_1(void)
{
    unit const r_3 = unit_impl;
    fwrite("", 1, 0, stdout);
    return lambda_2;
}
static string_ref lambda_2(string_ref const arg0)
{
    unit const r_4 = unit_impl;
    fwrite("", 1, 0, stdout);
    string_ref_add_reference(&arg0);
    return arg0;
}
int main(void)
{
    type_definition_0 const r_1 = lambda_1();
    string_ref const r_7 = lambda_2(string_literal("a", 1));
    bool const r_9 = string_ref_equals(r_7, string_literal("a", 1));
    unit const r_10 = assert_impl(r_9);
    string_ref_free(&r_7);
    return 0;
}
