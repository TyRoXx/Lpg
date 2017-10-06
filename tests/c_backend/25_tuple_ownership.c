#include <lpg_std_unit.h>
#include <lpg_std_assert.h>
#include <lpg_std_string.h>
#include <stdio.h>
#include <lpg_std_read.h>
typedef struct type_definition_0
{
    string_ref e_0;
    string_ref e_1;
} type_definition_0;
static type_definition_0 lambda_1(void);
static type_definition_0 lambda_1(void)
{
    unit const r_3 = unit_impl;
    fwrite("", 1, 0, stdout);
    string_ref const r_8 = read_impl();
    string_ref const result_10 = string_ref_concat(r_8, string_literal("123", 3));
    string_ref_add_reference(&result_10);
    string_ref_add_reference(&result_10);
    type_definition_0 const r_11 = {result_10, result_10};
    string_ref_free(&r_8);
    string_ref_free(&result_10);
    return r_11;
}
int main(void)
{
    type_definition_0 const tuple_1 = lambda_1();
    string_ref const r_7 = tuple_1.e_0;
    bool const r_8 = string_ref_equals(string_literal("123", 3), r_7);
    unit const r_9 = assert_impl(r_8);
    string_ref_free(&tuple_1.e_0);
    string_ref_free(&tuple_1.e_1);
    return 0;
}
