#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <stdint.h>
#include <lpg_std_integer.h>
typedef uint64_t (*type_definition_0)();
static uint64_t lambda_1(void);
static uint64_t lambda_1(void)
{
    /*side effect*/
    unit const r_2 = unit_impl;
    uint64_t const r_3 = 123;
    return r_3;
}
int main(void)
{
    type_definition_0 const f_0 = lambda_1;
    uint64_t const r_5 = 122;
    uint64_t const r_6 = lambda_1();
    bool const r_7 = integer_less(r_5, r_6);
    unit const r_8 = assert_impl(r_7);
    return 0;
}
