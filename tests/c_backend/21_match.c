#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <lpg_std_string.h>
static string_ref lambda_1(void);
static string_ref lambda_1(void)
{
    /*side effect*/
    unit const r_2 = unit_impl;
    return string_literal("b", 1);
}
int main(void)
{
    string_ref const r_5 = lambda_1();
    bool const r_7 = string_ref_equals(r_5, string_literal("a", 1));
    size_t r_12;
    if (0 == r_7)
    {
        r_12 = 1;
    }
    else
    {
        r_12 = 0;
    }
    unit const r_13 = assert_impl(r_12);
    string_ref_free(&r_5);
    return 0;
}
