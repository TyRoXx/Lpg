#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <stdio.h>
#include <stdint.h>
#include <lpg_std_integer.h>
static uint64_t lambda_1(uint64_t const a_0);
static uint64_t lambda_1(uint64_t const a_0)
{
    unit const r_4 = unit_impl;
    fwrite("", 1, 0, stdout);
    return a_0;
}
int main(void)
{
    uint64_t const i_2 = lambda_1(2);
    bool const r_8 = integer_equals(i_2, 2);
    unit const r_9 = assert_impl(r_8);
    return 0;
}
