#include "lpg_allocate.h"
#include "test_save_expression.h"
#include "test.h"
#include "test_integer.h"
#include "test_decode_string_literal.h"
#include "test_unicode_string.h"
#include "test_allocator.h"
#include "test_arithmetic.h"
#include "test_stream_writer.h"
#include "test_identifier.h"
#include "test_tokenize.h"
#include "test_unicode_view.h"
#include "test_parse_expression_success.h"
#include "test_parse_expression_syntax_error.h"
#include "test_expression.h"
#include "test_semantics.h"
#include "test_interprete.h"
#include "test_cli.h"
#include <stdio.h>
#if LPG_WITH_VLD
#include <vld.h>
#endif

int main(void)
{
    static void (*tests[])(void) = {test_integer,
                                    test_allocator,
                                    test_unicode_string,
                                    test_unicode_view,
                                    test_decode_string_literal,
                                    test_arithmetic,
                                    test_stream_writer,
                                    test_identifier,
                                    test_expression,
                                    test_save_expression,
                                    test_tokenize,
                                    test_parse_expression_success,
                                    test_parse_expression_syntax_error,
                                    test_semantics,
                                    test_interprete,
                                    test_cli};
    for (size_t i = 0; i < (sizeof(tests) / sizeof(*tests)); ++i)
    {
        tests[i]();
    }
    printf("Dynamic allocations: %zu\n", count_total_allocations());
    return lpg_print_test_summary();
}