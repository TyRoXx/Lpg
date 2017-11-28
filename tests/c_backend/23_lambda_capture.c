#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <lpg_std_string.h>
#include <stdio.h>
typedef struct type_definition_0
{
    size_t e_0;
} type_definition_0;
typedef type_definition_0 (*type_definition_1)(size_t);
static type_definition_0 lambda_1(size_t const a_0);
static size_t lambda_2(type_definition_0 const *const captures);
static type_definition_0 lambda_1(size_t const a_0)
{
    string_ref const r_3 = string_literal("", 0);
    unit const r_4 = unit_impl;
    fwrite(r_3.data, 1, r_3.length, stdout);
    type_definition_0 const r_5 = {a_0};
    return r_5;
}
static size_t lambda_2(type_definition_0 const *const captures)
{
    string_ref const r_2 = string_literal("", 0);
    unit const r_3 = unit_impl;
    fwrite(r_2.data, 1, r_2.length, stdout);
    return captures->e_0;
}
int main(void)
{
    type_definition_1 const f_0 = lambda_1;
    size_t const r_3 = 1;
    type_definition_0 const r_4 = lambda_1(r_3);
    size_t const r_5 = lambda_2(&r_4);
    unit const r_6 = assert_impl(r_5);
    return 0;
}
