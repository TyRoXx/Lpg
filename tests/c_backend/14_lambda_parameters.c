#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <stdio.h>
static size_t lambda_1(size_t const a_0, size_t const b_1);
static size_t lambda_1(size_t const a_0, size_t const b_1)
{
    unit const r_5 = unit_impl;
    fwrite("", 1, 0, stdout);
    size_t const r_12 = !b_1;
    size_t const r_13 = (a_0 & r_12);
    size_t const r_18 = !a_0;
    size_t const r_19 = (r_18 & b_1);
    size_t const r_20 = (r_13 | r_19);
    return r_20;
}
int main(void)
{
    size_t const r_5 = lambda_1(1, 0);
    unit const r_6 = assert_impl(r_5);
    size_t const r_11 = lambda_1(0, 1);
    unit const r_12 = assert_impl(r_11);
    size_t const r_19 = lambda_1(1, 1);
    size_t const r_20 = !r_19;
    unit const r_21 = assert_impl(r_20);
    size_t const r_28 = lambda_1(0, 0);
    size_t const r_29 = !r_28;
    unit const r_30 = assert_impl(r_29);
    return 0;
}
