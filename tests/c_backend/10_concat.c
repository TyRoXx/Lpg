#include <lpg_std_unit.h>
#include <lpg_std_string.h>
#include <stdio.h>
#include <lpg_std_read.h>
int main(void)
{
    string_ref const r_7 = read_impl();
    string_ref const r_8 = string_ref_concat(string_literal("a", 1), r_7);
    unit const r_9 = unit_impl;
    fwrite(r_8.data, 1, r_8.length, stdout);
    string_ref_free(&r_7);
    string_ref_free(&r_8);
    return 0;
}
