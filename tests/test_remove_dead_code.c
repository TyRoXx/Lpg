#include "test_remove_dead_code.h"
#include "test.h"
#include "print_instruction.h"
#include "lpg_remove_dead_code.h"
#include <string.h>
#include <stdio.h>
#include "lpg_check.h"
#include "handle_parse_error.h"
#include "lpg_standard_library.h"
#include "lpg_instruction.h"
#include "lpg_allocate.h"

static sequence parse(char const *input)
{
    test_parser_user user = {
        {input, strlen(input), source_location_create(0, 0)}, NULL, 0};
    expression_parser parser =
        expression_parser_create(find_next_token, handle_error, &user);
    sequence const result = parse_program(&parser);
    REQUIRE(user.base.remaining_size == 0);
    return result;
}

static void expect_no_errors(semantic_error const error, void *user)
{
    (void)error;
    (void)user;
    FAIL();
}

static void check_single_wellformed_function(
    char const *const source, structure const non_empty_global,
    instruction *const expected_body_elements, size_t const expected_body_size)
{
    sequence root = parse(source);
    checked_program checked =
        check(root, non_empty_global, expect_no_errors, NULL);
    sequence_free(&root);
    REQUIRE(checked.function_count == 1);
    remove_dead_code(&checked);
    instruction_sequence const expected_body =
        instruction_sequence_create(expected_body_elements, expected_body_size);
    if (!instruction_sequence_equals(
            &expected_body, &checked.functions[0].body))
    {
        printf("Expected instruction sequence:\n");
        print_instruction_sequence(expected_body);
        printf("Got instruction sequence:\n");
        print_instruction_sequence(checked.functions[0].body);
        FAIL();
    }
    checked_program_free(&checked);
    instruction_sequence_free(&expected_body);
}

void test_remove_dead_code(void)
{
    standard_library_description const std_library =
        describe_standard_library();

    {
        instruction const expected_main_function[] = {
            instruction_create_literal(
                literal_instruction_create(0, value_from_unit()))};
        check_single_wellformed_function(
            "let t = 1\n"
            "let v = {t}\n"
            "let w = {{{v}}}\n",
            std_library.globals, LPG_COPY_ARRAY(expected_main_function));
    }

    standard_library_description_free(&std_library);
}