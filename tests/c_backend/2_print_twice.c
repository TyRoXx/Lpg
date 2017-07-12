#include <lpg_std_unit.h>
#include <stdio.h>
int main(void)
{
    unit const r_3 = unit_impl;
    fwrite("Hello, ", 1, 7, stdout);
    unit const r_7 = unit_impl;
    fwrite("world!\n", 1, 7, stdout);
    return 0;
}
