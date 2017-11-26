#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <stdint.h>
#include <lpg_std_integer.h>
static uint64_t lambda_1(void);
static uint64_t lambda_1(void)
{
    /*side effect*/
    unit const r_2 = unit_impl;
    return 123;
}
int main(void)
{
    uint64_t const r_6 = lambda_1();
    bool const r_7 = integer_less(122, r_6);
    unit const r_8 = assert_impl(r_7);
    return 0;
}
