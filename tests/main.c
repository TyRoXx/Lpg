#include "lpg_allocate.h"
#include "lpg_monotonic_clock.h"
#include "test_save_expression.h"
#include "test.h"
#include "test_blob.h"
#include "test_integer.h"
#include "test_integer_range.h"
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
#include "test_instruction.h"
#include "test_semantics.h"
#include "test_interpreter.h"
#include "test_cli.h"
#include "test_c_backend.h"
#include "test_implicitly_convertible.h"
#include "test_remove_unused_functions.h"
#include "test_remove_dead_code.h"
#include "test_semantic_errors.h"
#include "test_value.h"
#include "test_type.h"
#include "test_path.h"
#include "test_thread.h"
#include <stdio.h>
#include <inttypes.h>
#if LPG_WITH_VLD
#include <vld.h>
#endif

int main(void)
{
    duration const started_at = read_monotonic_clock();
    static void (*tests[])(void) = {test_thread,
                                    test_blob,
                                    test_path,
                                    test_integer,
                                    test_integer_range,
                                    test_allocator,
                                    test_semantic_errors,
                                    test_unicode_string,
                                    test_unicode_view,
                                    test_decode_string_literal,
                                    test_arithmetic,
                                    test_instruction,
                                    test_stream_writer,
                                    test_identifier,
                                    test_expression,
                                    test_save_expression,
                                    test_tokenize,
                                    test_parse_expression_success,
                                    test_parse_expression_syntax_error,
                                    test_semantics,
                                    test_implicitly_convertible,
                                    test_cli,
                                    test_blob,
                                    test_c_backend,
                                    test_interpreter,
                                    test_value,
                                    test_remove_unused_functions,
                                    test_remove_dead_code,
                                    test_type};
    for (size_t i = 0; i < (sizeof(tests) / sizeof(*tests)); ++i)
    {
        tests[i]();
    }
    duration const finished_at = read_monotonic_clock();
    printf("Test duration: %" PRIu64 " ms\n", absolute_duration_difference(started_at, finished_at).milliseconds);
    printf("Dynamic allocations: %zu\n", count_total_allocations());
    if (count_active_allocations() == 0)
    {
        return lpg_print_test_summary();
    }
    printf("Detected memory leak\n");
    lpg_print_test_summary();
    return 1;
}
