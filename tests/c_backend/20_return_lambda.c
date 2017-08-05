#include <lpg_std_assert.h>
#include <lpg_std_string.h>
typedef string_ref (*type_definition_0)();
static type_definition_0 lambda_1(void);
static string_ref lambda_2(void);
static type_definition_0 lambda_1(void)
{
    return lambda_2;
}
static string_ref lambda_2(void)
{
    return string_literal("a", 1);
}
int main(void)
{
    unit const r_5 = assert_impl(1);
    return 0;
}
