#include <lpg_std_assert.h>
static size_t lambda_1(size_t const arg0, size_t const arg1)
{
    size_t const r_8 = !arg1;
    size_t const r_9 = (arg0 & r_8);
    size_t const r_14 = !arg0;
    size_t const r_15 = (r_14 & arg1);
    size_t const r_16 = (r_9 | r_15);
    return r_16;
}
int main(void)
{
    unit const r_4 = assert_impl(1);
    unit const r_8 = assert_impl(1);
    unit const r_12 = assert_impl(1);
    unit const r_16 = assert_impl(1);
    return 0;
}
