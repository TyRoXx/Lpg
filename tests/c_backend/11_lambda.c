#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <stdio.h>
static size_t lambda_1(void);
static size_t lambda_1(void)
{
    unit const r_3 = unit_impl;
    fwrite("", 1, 0, stdout);
    return 1;
}
int main(void)
{
    size_t const r_3 = lambda_1();
    unit const r_4 = assert_impl(r_3);
    return 0;
}
