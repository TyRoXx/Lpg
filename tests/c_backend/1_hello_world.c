#include <lpg_std_unit.h>
#include <stdio.h>
int main(void)
{
    unit const r_3 = unit_impl;
    fwrite("Hello, world!", 1, 13, stdout);
    return 0;
}
