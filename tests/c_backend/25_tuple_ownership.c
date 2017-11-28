#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <lpg_std_string.h>
#include <stdio.h>
typedef struct type_definition_0
{
    string_ref e_0;
    string_ref e_1;
} type_definition_0;
typedef string_ref (*type_definition_1)();
typedef type_definition_0 (*type_definition_2)();
static type_definition_0 lambda_1(void);
static string_ref lambda_2(void);
static type_definition_0 lambda_1(void)
{
    type_definition_1 const read_0 = lambda_2;
    string_ref const r_3 = string_literal("", 0);
    unit const r_4 = unit_impl;
    fwrite(r_3.data, 1, r_3.length, stdout);
    string_ref const r_7 = lambda_2();
    string_ref const r_8 = string_literal("123", 3);
    string_ref const result_9 = string_ref_concat(r_7, r_8);
    string_ref_add_reference(&result_9);
    string_ref_add_reference(&result_9);
    type_definition_0 const r_10 = {result_9, result_9};
    string_ref_free(&r_7);
    string_ref_free(&result_9);
    return r_10;
}
static string_ref lambda_2(void)
{
    /*side effect*/
    unit const r_2 = unit_impl;
    string_ref const r_3 = string_literal("", 0);
    string_ref_add_reference(&r_3);
    return r_3;
}
int main(void)
{
    type_definition_2 const construct_0 = lambda_1;
    type_definition_0 const tuple_1 = lambda_1();
    string_ref const r_6 = string_literal("123", 3);
    string_ref const r_7 = tuple_1.e_0;
    bool const r_8 = string_ref_equals(r_6, r_7);
    unit const r_9 = assert_impl(r_8);
    string_ref_free(&tuple_1.e_0);
    string_ref_free(&tuple_1.e_1);
    return 0;
}
