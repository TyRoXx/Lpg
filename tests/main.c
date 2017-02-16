#include "lpg_allocate.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define REQUIRE(x)                                                             \
    if (!(x))                                                                  \
    {                                                                          \
        abort();                                                               \
    }

int main(void)
{
    deallocate(allocate(0));
    {
        char *m = allocate(1);
        REQUIRE(m);
        *m = 'a';
        deallocate(m);
    }
    {
        size_t const size = 1000;
        char *m = allocate(size);
        REQUIRE(m);
        memset(m, 'a', size);
        deallocate(m);
    }
    {
        char *m = allocate(1);
        *m = 'a';
        m = reallocate(m, 2);
        m[1] = 'b';
        REQUIRE(m[0] == 'a');
        REQUIRE(m[1] == 'b');
        deallocate(m);
    }
    {
        char *m = reallocate(NULL, 1);
        *m = 'a';
        deallocate(m);
    }
    deallocate(allocate_array(0, 0));
    deallocate(allocate_array(0, 1));
    deallocate(allocate_array(1, 0));
    {
        char *m = allocate_array(1, 1);
        REQUIRE(m);
        *m = 'a';
        deallocate(m);
    }
    {
        char *m = allocate_array(1, 1);
        REQUIRE(m);
        *m = 'a';
        m = reallocate_array(m, 2, 2);
        m[1] = 'b';
        m[2] = 'c';
        m[3] = 'd';
        REQUIRE(m[0] == 'a');
        REQUIRE(m[1] == 'b');
        REQUIRE(m[2] == 'c');
        REQUIRE(m[3] == 'd');
        deallocate(m);
    }
    {
        char *m = reallocate_array(NULL, 2, 2);
        m[0] = 'a';
        m[1] = 'b';
        m[2] = 'c';
        m[3] = 'd';
        REQUIRE(m[0] == 'a');
        REQUIRE(m[1] == 'b');
        REQUIRE(m[2] == 'c');
        REQUIRE(m[3] == 'd');
        deallocate(m);
    }
    check_allocations();
    printf("All tests passed\n");
    return 0;
}
