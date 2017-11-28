#include <lpg_std_unit.h>
#include <lpg_std_string.h>
#include <stdio.h>
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
    string_ref const r_5 = string_literal("a", 1);
    string_ref const r_6 = lambda_1();
    string_ref const r_7 = string_ref_concat(r_5, r_6);
    unit const r_8 = unit_impl;
    fwrite(r_7.data, 1, r_7.length, stdout);
    string_ref_free(&r_6);
    string_ref_free(&r_7);
    return 0;
}
