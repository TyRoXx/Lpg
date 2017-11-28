#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <lpg_std_string.h>
typedef string_ref (*type_definition_0)();
static string_ref lambda_1(void);
static string_ref lambda_1(void)
{
    /*side effect*/
    unit const r_2 = unit_impl;
    string_ref const r_3 = string_literal("b", 1);
    string_ref_add_reference(&r_3);
    return r_3;
}
int main(void)
{
    type_definition_0 const read_0 = lambda_1;
    string_ref const r_5 = lambda_1();
    string_ref const r_6 = string_literal("a", 1);
    bool const r_7 = string_ref_equals(r_5, r_6);
    size_t const r_8 = 0;
    size_t const r_10 = 1;
    size_t r_12;
    if (r_8 == r_7)
    {
        size_t const r_9 = 1;
        r_12 = r_9;
    }
    else
    {
        size_t const r_11 = 0;
        r_12 = r_11;
    }
    unit const r_13 = assert_impl(r_12);
    string_ref_free(&r_5);
    return 0;
}
