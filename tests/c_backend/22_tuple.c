#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <stdio.h>
typedef unit type_definition_0;
typedef struct type_definition_1
{
    size_t e_0;
    size_t e_1;
    type_definition_0 e_2;
}
type_definition_1;
static size_t lambda_1(void);
static size_t lambda_1(void)
{
    unit const r_3 = unit_impl;
    fwrite("", 1, 0, stdout);
    return 1;
}
int main(void)
{
    size_t const r_5 = lambda_1();
    type_definition_0 const r_6 = {0};
    type_definition_1 const r_7 = {0, r_5, r_6};
    size_t const r_3 = r_7.e_1;
    unit const r_8 = assert_impl(r_3);
    return 0;
}
