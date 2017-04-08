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
#include "lpg_assert.h"
#include "test_tokenize.h"
#include "test_parse_expression.h"
#include "test_unicode_view.h"
#if LPG_WITH_VLD
#include <vld.h>
static void check_allocations_maybe(void)
{
}
#else
static void check_allocations_maybe(void)
{
    ASSERT(count_active_allocations() == 0);
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
    test_tokenize();
    check_allocations_maybe();
    test_parse_expression();
    check_allocations_maybe();
    test_unicode_view();
    check_allocations_maybe();
    printf("Dynamic allocations: %zu\n", count_total_allocations());
    return lpg_print_test_summary();
}
