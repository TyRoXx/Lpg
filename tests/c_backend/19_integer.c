#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <lpg_std_string.h>
#include <stdio.h>
#include <stdint.h>
#include <lpg_std_integer.h>
typedef uint64_t (*type_definition_0)(uint64_t);
static uint64_t lambda_1(uint64_t const a_0);
static uint64_t lambda_1(uint64_t const a_0)
{
    string_ref const r_3 = string_literal("", 0);
    unit const r_4 = unit_impl;
    fwrite(r_3.data, 1, r_3.length, stdout);
    return a_0;
}
int main(void)
{
    type_definition_0 const s_0 = lambda_1;
    uint64_t const r_1 = 2;
    uint64_t const i_2 = lambda_1(r_1);
    uint64_t const r_7 = 2;
    bool const r_8 = integer_equals(i_2, r_7);
    unit const r_9 = assert_impl(r_8);
    return 0;
}
