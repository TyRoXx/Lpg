#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <stdio.h>
static size_t lambda_1(size_t const arg0);
static size_t lambda_1(size_t const arg0)
{
    unit const r_4 = unit_impl;
    fwrite("", 1, 0, stdout);
    return arg0;
}
int main(void)
{
    size_t const r_4 = lambda_1(1);
    unit const r_5 = assert_impl(r_4);
    return 0;
}
