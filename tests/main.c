#include "lpg_allocate.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include "lpg_arithmetic.h"
#include "test_save_expression.h"
#include "test.h"
#include "test_integer.h"
#include "test_unicode_string.h"
#include "test_allocator.h"
#include "test_arithmetic.h"
#include "test_stream_writer.h"

int main(void)
{
    check_allocations();
    test_integer();
    check_allocations();
    test_save_expression();
    check_allocations();
    test_unicode_string();
    check_allocations();
    test_allocator();
    check_allocations();
    test_arithmetic();
    check_allocations();
    test_stream_writer();
    check_allocations();
    return lpg_print_test_summary();
}
