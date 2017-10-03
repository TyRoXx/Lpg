#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <stdio.h>
typedef struct type_definition_0
{
    size_t e_0;
} type_definition_0;
static type_definition_0 lambda_1(size_t const a_0);
static size_t lambda_2(type_definition_0 const *const captures);
static type_definition_0 lambda_1(size_t const a_0)
{
    unit const r_4 = unit_impl;
    fwrite("", 1, 0, stdout);
    type_definition_0 const r_5 = {a_0};
    return r_5;
}
static size_t lambda_2(type_definition_0 const *const captures)
{
    unit const r_3 = unit_impl;
    fwrite("", 1, 0, stdout);
    return captures->e_0;
}
int main(void)
{
    type_definition_0 const r_4 = lambda_1(1);
    size_t const r_5 = lambda_2(&r_4);
    unit const r_6 = assert_impl(r_5);
    return 0;
}
