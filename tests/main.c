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
#include "test_identifier.h"
#if LPG_WITH_VLD
#include <vld.h>
static void check_allocations_maybe(void)
{
}
#else
static void check_allocations_maybe(void)
{
    check_allocations();
}
#endif

int main(void)
{
    check_allocations_maybe();
    test_integer();
    check_allocations_maybe();
    test_save_expression();
    check_allocations_maybe();
    test_unicode_string();
    check_allocations_maybe();
    test_allocator();
    check_allocations_maybe();
    test_arithmetic();
    check_allocations_maybe();
    test_stream_writer();
    check_allocations_maybe();
    test_identifier();
    check_allocations_maybe();
    return lpg_print_test_summary();
}
