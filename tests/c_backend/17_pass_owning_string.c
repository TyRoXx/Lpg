#include <lpg_std_unit.h>
#include <lpg_std_string.h>
#include <stdio.h>
typedef string_ref (*type_definition_0)();
typedef unit (*type_definition_1)(string_ref);
static string_ref lambda_1(void);
static unit lambda_2(string_ref const a_0);
static string_ref lambda_1(void)
{
    /*side effect*/
    unit const r_2 = unit_impl;
    string_ref const r_3 = string_literal("b", 1);
    string_ref_add_reference(&r_3);
    return r_3;
}
static unit lambda_2(string_ref const a_0)
{
    unit const r_3 = unit_impl;
    fwrite(a_0.data, 1, a_0.length, stdout);
    return r_3;
}
int main(void)
{
    type_definition_0 const read_0 = lambda_1;
    type_definition_1 const s_1 = lambda_2;
    string_ref const r_4 = lambda_1();
    string_ref const r_5 = string_literal("c", 1);
    string_ref const r_6 = string_ref_concat(r_4, r_5);
    unit const r_7 = lambda_2(r_6);
    string_ref_free(&r_4);
    string_ref_free(&r_6);
    return 0;
}
