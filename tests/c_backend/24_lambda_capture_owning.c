#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <lpg_std_string.h>
#include <stdio.h>
typedef struct type_definition_0
{
    string_ref e_0;
} type_definition_0;
static type_definition_0 lambda_1(string_ref const arg0);
static string_ref lambda_2(type_definition_0 const *const captures);
static type_definition_0 lambda_1(string_ref const arg0)
{
    unit const r_4 = unit_impl;
    fwrite("", 1, 0, stdout);
    type_definition_0 const r_5 = {arg0};
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
    type_definition_0 const r_7 = lambda_1(string_literal("a", 1));
    string_ref const r_8 = lambda_2(&r_7);
    bool const r_9 = string_ref_equals(string_literal("a", 1), r_8);
    unit const r_10 = assert_impl(r_9);
    string_ref_free(&r_8);
    return 0;
}
