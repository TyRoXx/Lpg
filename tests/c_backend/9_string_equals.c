#include <lpg_std_assert.h>
#include <lpg_std_string.h>
static string_ref lambda_1(void);
static string_ref lambda_1(void)
{
    /*side effect*/
    return string_literal("", 0);
}
int main(void)
{
    string_ref const r_5 = lambda_1();
    bool const r_7 = string_ref_equals(r_5, string_literal("", 0));
    unit const r_8 = assert_impl(r_7);
    string_ref_free(&r_5);
    return 0;
}
