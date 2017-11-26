#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <lpg_std_string.h>
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
    uint64_t const r_8 = lambda_1();
    string_ref const r_9 = integer_to_string(r_8);
    bool const r_10 = string_ref_equals(string_literal("123", 3), r_9);
    unit const r_11 = assert_impl(r_10);
    string_ref_free(&r_9);
    return 0;
}
