#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <lpg_std_string.h>
#include <stdio.h>
typedef size_t (*type_definition_0)(size_t);
static size_t lambda_1(size_t const a_0);
static size_t lambda_1(size_t const a_0)
{
    string_ref const r_3 = string_literal("", 0);
    unit const r_4 = unit_impl;
    fwrite(r_3.data, 1, r_3.length, stdout);
    return a_0;
}
int main(void)
{
    type_definition_0 const id_0 = lambda_1;
    size_t const r_3 = 1;
    size_t const r_4 = lambda_1(r_3);
    unit const r_5 = assert_impl(r_4);
    return 0;
}
