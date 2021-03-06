#include "test_remove_dead_code.h"
#include "find_builtin_module_directory.h"
#include "handle_parse_error.h"
#include "lpg_allocate.h"
#include "lpg_check.h"
#include "lpg_instruction.h"
#include "lpg_remove_dead_code.h"
#include "lpg_standard_library.h"
#include "print_instruction.h"
#include "test.h"
#include <stdio.h>
#include <string.h>

static sequence parse(char const *input, expression_pool *const pool)
{
    test_parser_user user = {{input, strlen(input), source_location_create(0, 0)}, NULL, 0};
    expression_parser parser = expression_parser_create(&user.base, handle_error, &user, pool);
    sequence const result = parse_program(&parser);
    expression_parser_free(parser);
    REQUIRE(user.base.remaining_size == 0);
    return result;
}

static void expect_no_errors(complete_semantic_error const error, void *user)
{
    (void)error;
    (void)user;
    FAIL();
}

static void expect_no_complete_parse_error(complete_parse_error error, callback_user user)
{
    (void)error;
    (void)user;
    FAIL();
}

static void check_single_wellformed_function(char const *const source, structure const non_empty_global,
                                             instruction *const expected_body_elements, size_t const expected_body_size)
{
    expression_pool pool = expression_pool_create();
    sequence root = parse(source, &pool);
    unicode_string const module_directory = find_builtin_module_directory();
    module_loader loader =
        module_loader_create(unicode_view_from_string(module_directory), expect_no_complete_parse_error, NULL);
    source_file_lines_owning const lines = source_file_lines_owning_scan(unicode_view_from_c_str(source));
    checked_program checked =
        check(root, non_empty_global, expect_no_errors, &loader,
              source_file_create(unicode_view_from_c_str("test.lpg"), unicode_view_from_c_str(source),
                                 source_file_lines_from_owning(lines)),
              unicode_view_from_string(module_directory), 100000, NULL);
    sequence_free(&root);
    REQUIRE(checked.function_count == 1);
    remove_dead_code(&checked);
    instruction_sequence const expected_body =
        instruction_sequence_create(expected_body_elements, expected_body_size, 0);
    if (!instruction_sequence_equals(&expected_body, &checked.functions[0].body))
    {
        printf("Expected instruction sequence:\n");
        print_instruction_sequence(expected_body, 0);
        printf("Got instruction sequence:\n");
        print_instruction_sequence(checked.functions[0].body, 0);
        FAIL();
    }
    checked_program_free(&checked);
    unicode_string_free(&module_directory);
    instruction_sequence_free(&expected_body);
    source_file_lines_owning_free(lines);
    expression_pool_free(pool);
}

void test_remove_dead_code(void)
{
    standard_library_description const std_library = describe_standard_library();

    {
        instruction const expected_main_function[] = {
            instruction_create_literal(literal_instruction_create(0, value_from_unit(), type_from_unit())),
            instruction_create_return(return_instruction_create(0, 1))};
        check_single_wellformed_function("let s = struct\n"
                                         "    i: int(1, 1)\n"
                                         "let r = struct\n"
                                         "    s: s\n"
                                         "let t = 1\n"
                                         "let v = s{t}\n"
                                         "let w = r{v}\n",
                                         std_library.globals, LPG_COPY_ARRAY(expected_main_function));
    }

    standard_library_description_free(&std_library);
}
