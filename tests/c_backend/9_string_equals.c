#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <lpg_std_string.h>
typedef string_ref (*type_definition_0)();
static string_ref lambda_1(void);
static string_ref lambda_1(void)
{
    /*side effect*/
    unit const r_2 = unit_impl;
    string_ref const r_3 = string_literal("", 0);
    string_ref_add_reference(&r_3);
    return r_3;
}
int main(void)
{
    type_definition_0 const read_0 = lambda_1;
    string_ref const r_5 = lambda_1();
    string_ref const r_6 = string_literal("", 0);
    bool const r_7 = string_ref_equals(r_5, r_6);
    unit const r_8 = assert_impl(r_7);
    string_ref_free(&r_5);
    return 0;
}
