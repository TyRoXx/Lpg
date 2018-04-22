#include "test_thread.h"
#include "test.h"
#include "lpg_thread.h"
#include "lpg_allocate.h"

static void trivial_thread_function(void *argument)
{
    int *cast = argument;
    REQUIRE(*cast == 123);
    *cast = 124;
    deallocate(allocate(1000));
}

void test_thread(void)
{
    for (size_t i = 0; i < 100; ++i)
    {
        int argument = 123;
        create_thread_result const result = create_thread(trivial_thread_function, &argument);
        REQUIRE(result.is_success == success_yes);
        join_thread(result.success);
        REQUIRE(argument == 124);
    }
}
