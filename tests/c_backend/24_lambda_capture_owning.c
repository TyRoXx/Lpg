#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <lpg_std_string.h>
#include <stdio.h>
typedef struct type_definition_0
{
    string_ref e_0;
} type_definition_0;
static type_definition_0 lambda_1(string_ref const a_0);
static string_ref lambda_2(type_definition_0 const *const captures);
static type_definition_0 lambda_1(string_ref const a_0)
{
    unit const r_4 = unit_impl;
    fwrite("", 1, 0, stdout);
    type_definition_0 const r_5 = {a_0};
    return r_5;
}
static string_ref lambda_2(type_definition_0 const *const captures)
{
    unit const r_3 = unit_impl;
    fwrite("", 1, 0, stdout);
    return captures->e_0;
}
int main(void)
{
    type_definition_0 const inner_2 = lambda_1(string_literal("a", 1));
    string_ref const result_3 = lambda_2(&inner_2);
    bool const r_9 = string_ref_equals(string_literal("a", 1), result_3);
    unit const r_10 = assert_impl(r_9);
    string_ref_free(&result_3);
    return 0;
}
