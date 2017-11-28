#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
typedef unit (*type_definition_0)();
static unit lambda_1(void);
static unit lambda_1(void)
{
    size_t const r_2 = 1;
    unit const r_3 = assert_impl(r_2);
    return r_3;
}
int main(void)
{
    type_definition_0 const f_0 = lambda_1;
    unit const r_1 = lambda_1();
    return 0;
}
