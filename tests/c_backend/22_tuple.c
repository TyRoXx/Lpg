#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <lpg_std_string.h>
#include <stdio.h>
typedef size_t (*type_definition_0)();
typedef unit type_definition_1;
typedef struct type_definition_2
{
    size_t e_0;
    size_t e_1;
    type_definition_1 e_2;
} type_definition_2;
static size_t lambda_1(void);
static size_t lambda_1(void)
{
    string_ref const r_2 = string_literal("", 0);
    unit const r_3 = unit_impl;
    fwrite(r_2.data, 1, r_2.length, stdout);
    size_t const r_4 = 1;
    return r_4;
}
int main(void)
{
    type_definition_0 const f_0 = lambda_1;
    size_t const r_4 = 0;
    size_t const r_5 = lambda_1();
    type_definition_1 const r_6 = {0};
    type_definition_2 const r_7 = {r_4, r_5, r_6};
    size_t const r_3 = r_7.e_1;
    unit const r_8 = assert_impl(r_3);
    return 0;
}
