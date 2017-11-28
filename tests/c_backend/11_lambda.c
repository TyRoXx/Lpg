#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <lpg_std_string.h>
#include <stdio.h>
typedef size_t (*type_definition_0)();
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
    size_t const r_3 = lambda_1();
    unit const r_4 = assert_impl(r_3);
    return 0;
}
