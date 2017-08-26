#include <lpg_std_assert.h>
#include <lpg_std_string.h>
typedef string_ref (*type_definition_0)();
typedef string_ref (*type_definition_1)(unit);
typedef unit (*type_definition_2)(unit, unit);
static type_definition_0 lambda_1(void);
static string_ref lambda_2(void);
static type_definition_1 lambda_3(void);
static string_ref lambda_4(unit const arg0);
static type_definition_2 lambda_5(void);
static unit lambda_6(unit const arg0, unit const arg1);
static type_definition_0 lambda_1(void)
{
    return lambda_2;
}
static string_ref lambda_2(void)
{
    return string_literal("a", 1);
}
static type_definition_1 lambda_3(void)
{
    return lambda_4;
}
static string_ref lambda_4(unit const arg0)
{
    return string_literal("a", 1);
}
static type_definition_2 lambda_5(void)
{
    return lambda_6;
}
static unit lambda_6(unit const arg0, unit const arg1)
{
    return unit_value;
}
int main(void)
{
    unit const r_7 = assert_impl(1);
    return 0;
}
