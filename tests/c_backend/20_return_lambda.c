#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <lpg_std_string.h>
#include <stdio.h>
typedef string_ref (*type_definition_0)(string_ref);
typedef type_definition_0 (*type_definition_1)();
static type_definition_0 lambda_1(void);
static string_ref lambda_2(string_ref const a_0);
static type_definition_0 lambda_1(void)
{
    string_ref const r_2 = string_literal("", 0);
    unit const r_3 = unit_impl;
    fwrite(r_2.data, 1, r_2.length, stdout);
    type_definition_0 const r_4 = lambda_2;
    return r_4;
}
static string_ref lambda_2(string_ref const a_0)
{
    string_ref const r_3 = string_literal("", 0);
    unit const r_4 = unit_impl;
    fwrite(r_3.data, 1, r_3.length, stdout);
    string_ref_add_reference(&a_0);
    return a_0;
}
int main(void)
{
    type_definition_1 const s_0 = lambda_1;
    type_definition_0 const r_1 = lambda_1();
    string_ref const r_6 = string_literal("a", 1);
    string_ref const r_7 = lambda_2(r_6);
    string_ref const r_8 = string_literal("a", 1);
    bool const r_9 = string_ref_equals(r_7, r_8);
    unit const r_10 = assert_impl(r_9);
    string_ref_free(&r_7);
    return 0;
}
