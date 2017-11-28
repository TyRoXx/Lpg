#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <lpg_std_string.h>
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
    string_ref const r_5 = string_literal("123", 3);
    uint64_t const r_8 = lambda_1();
    string_ref const r_9 = integer_to_string(r_8);
    bool const r_10 = string_ref_equals(r_5, r_9);
    unit const r_11 = assert_impl(r_10);
    string_ref_free(&r_9);
    return 0;
}
