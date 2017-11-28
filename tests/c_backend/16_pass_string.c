#include <lpg_std_unit.h>
#include <lpg_std_string.h>
#include <stdio.h>
typedef unit (*type_definition_0)(string_ref);
static unit lambda_1(string_ref const a_0);
static unit lambda_1(string_ref const a_0)
{
    unit const r_3 = unit_impl;
    fwrite(a_0.data, 1, a_0.length, stdout);
    return r_3;
}
int main(void)
{
    type_definition_0 const s_0 = lambda_1;
    string_ref const r_1 = string_literal("a", 1);
    unit const r_2 = lambda_1(r_1);
    return 0;
}
