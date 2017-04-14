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
#include "test_unicode_view.h"
#include "test_parse_expression_success.h"
#include "test_parse_expression_syntax_error.h"
#include "test_expression.h"
#include "test_semantics.h"
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
    static void (*tests[])(void) = {test_integer,
                                    test_allocator,
                                    test_unicode_string,
                                    test_unicode_view,
                                    test_arithmetic,
                                    test_stream_writer,
                                    test_identifier,
                                    test_expression,
                                    test_save_expression,
                                    test_tokenize,
                                    test_parse_expression_success,
                                    test_parse_expression_syntax_error,
                                    test_semantics};
    for (size_t i = 0; i < (sizeof(tests) / sizeof(*tests)); ++i)
    {
        tests[i]();
        check_allocations_maybe();
    }
    printf("Dynamic allocations: %zu\n", count_total_allocations());
    return lpg_print_test_summary();
}
