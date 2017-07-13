#include <lpg_std_unit.h>
#include <lpg_std_string.h>
#include <stdio.h>
#include <lpg_std_read.h>
int main(void)
{
    for (;;)
    {
        break;
        string_ref const r_4 = read_impl();
        unit const r_5 = unit_impl;
        fwrite(r_4.data, 1, r_4.length, stdout);
        string_ref_free(&r_4);
    }
    return 0;
}
