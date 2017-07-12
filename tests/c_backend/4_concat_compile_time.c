#include <lpg_std_unit.h>
#include <stdio.h>
int main(void)
{
    unit const r_3 = unit_impl;
    fwrite("123456", 1, 6, stdout);
    return 0;
}
