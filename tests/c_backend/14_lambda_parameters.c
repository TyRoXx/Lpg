#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <lpg_std_string.h>
#include <stdio.h>
typedef size_t (*type_definition_0)(size_t, size_t);
static size_t lambda_1(size_t const a_0, size_t const b_1);
static size_t lambda_1(size_t const a_0, size_t const b_1)
{
    string_ref const r_4 = string_literal("", 0);
    unit const r_5 = unit_impl;
    fwrite(r_4.data, 1, r_4.length, stdout);
    size_t const r_12 = !b_1;
    size_t const r_13 = (a_0 & r_12);
    size_t const r_18 = !a_0;
    size_t const r_19 = (r_18 & b_1);
    size_t const r_20 = (r_13 | r_19);
    return r_20;
}
int main(void)
{
    type_definition_0 const xor_0 = lambda_1;
    size_t const r_3 = 1;
    size_t const r_4 = 0;
    size_t const r_5 = lambda_1(r_3, r_4);
    unit const r_6 = assert_impl(r_5);
    size_t const r_9 = 0;
    size_t const r_10 = 1;
    size_t const r_11 = lambda_1(r_9, r_10);
    unit const r_12 = assert_impl(r_11);
    size_t const r_17 = 1;
    size_t const r_18 = 1;
    size_t const r_19 = lambda_1(r_17, r_18);
    size_t const r_20 = !r_19;
    unit const r_21 = assert_impl(r_20);
    size_t const r_26 = 0;
    size_t const r_27 = 0;
    size_t const r_28 = lambda_1(r_26, r_27);
    size_t const r_29 = !r_28;
    unit const r_30 = assert_impl(r_29);
    return 0;
}
