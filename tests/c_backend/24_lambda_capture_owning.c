#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <lpg_std_string.h>
#include <stdio.h>
typedef struct type_definition_0
{
    string_ref e_0;
} type_definition_0;
typedef type_definition_0 (*type_definition_1)(string_ref);
static type_definition_0 lambda_1(string_ref const a_0);
static string_ref lambda_2(type_definition_0 const *const captures);
static type_definition_0 lambda_1(string_ref const a_0)
{
    string_ref const r_3 = string_literal("", 0);
    unit const r_4 = unit_impl;
    fwrite(r_3.data, 1, r_3.length, stdout);
    string_ref_add_reference(&a_0);
    type_definition_0 const r_5 = {a_0};
    return r_5;
}
static string_ref lambda_2(type_definition_0 const *const captures)
{
    string_ref const r_2 = string_literal("", 0);
    unit const r_3 = unit_impl;
    fwrite(r_2.data, 1, r_2.length, stdout);
    return captures->e_0;
}
int main(void)
{
    type_definition_1 const outer_0 = lambda_1;
    string_ref const r_1 = string_literal("a", 1);
    type_definition_0 const inner_2 = lambda_1(r_1);
    string_ref const result_3 = lambda_2(&inner_2);
    string_ref const r_8 = string_literal("a", 1);
    bool const r_9 = string_ref_equals(r_8, result_3);
    unit const r_10 = assert_impl(r_9);
    string_ref_free(&inner_2.e_0);
    string_ref_free(&result_3);
    return 0;
}
