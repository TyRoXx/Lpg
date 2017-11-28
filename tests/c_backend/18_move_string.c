#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <lpg_std_string.h>
typedef string_ref (*type_definition_0)();
typedef string_ref (*type_definition_1)(string_ref);
static string_ref lambda_1(void);
static string_ref lambda_2(string_ref const a_0);
static string_ref lambda_1(void)
{
    /*side effect*/
    unit const r_2 = unit_impl;
    string_ref const r_3 = string_literal("", 0);
    string_ref_add_reference(&r_3);
    return r_3;
}
static string_ref lambda_2(string_ref const a_0)
{
    string_ref_add_reference(&a_0);
    return a_0;
}
int main(void)
{
    type_definition_0 const read_0 = lambda_1;
    type_definition_1 const s_1 = lambda_2;
    string_ref const r_4 = lambda_1();
    string_ref const r_5 = string_literal("", 0);
    string_ref const r_6 = string_ref_concat(r_4, r_5);
    string_ref const t_7 = lambda_2(r_6);
    string_ref const r_12 = string_literal("", 0);
    bool const r_13 = string_ref_equals(r_12, t_7);
    unit const r_14 = assert_impl(r_13);
    string_ref_free(&r_4);
    string_ref_free(&r_6);
    string_ref_free(&t_7);
    return 0;
}
