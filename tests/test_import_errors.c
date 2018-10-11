#include "test_import_errors.h"
#include "find_test_module_directory.h"
#include "handle_parse_error.h"
#include "lpg_allocate.h"
#include "lpg_check.h"
#include "lpg_instruction.h"
#include "lpg_standard_library.h"
#include "test.h"
#include <string.h>

static sequence parse(char const *input)
{
    test_parser_user user = {{input, strlen(input), source_location_create(0, 0)}, NULL, 0};
    expression_parser parser = expression_parser_create(find_next_token, &user, handle_error, &user);
    sequence const result = parse_program(&parser);
    REQUIRE(user.base.remaining_size == 0);
    return result;
}

typedef struct expected_semantic_errors
{
    complete_semantic_error const *errors;
    size_t count;
} expected_semantic_errors;

static void expect_errors(complete_semantic_error const error, void *user)
{
    expected_semantic_errors *expected = user;
    REQUIRE(expected->count >= 1);
    complete_semantic_error const expected_error = expected->errors[0];
    REQUIRE(complete_semantic_error_equals(error, expected_error));
    ++expected->errors;
    --expected->count;
}

typedef struct expected_parse_errors
{
    complete_parse_error const *errors;
    size_t count;
} expected_parse_errors;

static void expect_no_complete_parse_error(complete_parse_error error, callback_user user)
{
    expected_parse_errors *expected = user;
    REQUIRE(expected->count >= 1);
    REQUIRE(complete_parse_error_equals(error, expected->errors[0]));
    ++expected->errors;
    --expected->count;
}

static checked_program simple_check(sequence const root, structure const global, check_error_handler *on_error,
                                    void *user, unicode_view const module_directory,
                                    expected_parse_errors expected_errors, unicode_view const file_name,
                                    unicode_view const source)
{
    module_loader loader = module_loader_create(module_directory, expect_no_complete_parse_error, &expected_errors);
    return check(root, global, on_error, &loader, file_name, source, user);
}

void test_import_errors(void)
{
    unicode_string const module_directory = find_test_module_directory();
    unicode_view const module_directory_view = unicode_view_from_string(module_directory);
    standard_library_description const std_library = describe_standard_library();

    {
        sequence root = parse("import syntaxerror\n");
        unicode_view const importing_file = unicode_view_from_c_str("");
        unicode_view const importing_file_content = unicode_view_from_c_str("");
        complete_semantic_error const semantic_errors[] = {complete_semantic_error_create(
            semantic_error_create(semantic_error_import_failed, source_location_create(0, 0)), importing_file,
            importing_file_content)};
        expected_semantic_errors expected = {semantic_errors, LPG_ARRAY_SIZE(semantic_errors)};
        unicode_view const expected_file_name_pieces[] = {
            module_directory_view, unicode_view_from_c_str("syntaxerror.lpg")};
        unicode_string const expected_file_name =
            path_combine(expected_file_name_pieces, LPG_ARRAY_SIZE(expected_file_name_pieces));
        unicode_view const expected_source = unicode_view_from_c_str("let a = .\n");
        complete_parse_error const parse_errors[] = {
            complete_parse_error_create(
                parse_error_create(parse_error_expected_expression, source_location_create(0, 8)),
                unicode_view_from_string(expected_file_name), expected_source),
            complete_parse_error_create(
                parse_error_create(parse_error_expected_expression, source_location_create(0, 9)),
                unicode_view_from_string(expected_file_name), expected_source),
            complete_parse_error_create(
                parse_error_create(parse_error_expected_expression, source_location_create(1, 0)),
                unicode_view_from_string(expected_file_name), expected_source)};
        expected_parse_errors const expected_parse_errors_ = {parse_errors, LPG_ARRAY_SIZE(parse_errors)};
        checked_program checked =
            simple_check(root, std_library.globals, expect_errors, &expected, module_directory_view,
                         expected_parse_errors_, importing_file, importing_file_content);
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
        unicode_string_free(&expected_file_name);
    }

    {
        char const *const source = "import semanticerror\n";
        sequence root = parse(source);
        unicode_view const imported_file_pieces[] = {
            module_directory_view, unicode_view_from_c_str("semanticerror.lpg")};
        unicode_string const imported_file = path_combine(imported_file_pieces, LPG_ARRAY_SIZE(imported_file_pieces));
        unicode_view const imported_file_content = unicode_view_from_c_str("let a = unknown\n");
        unicode_view const importing_file = unicode_view_from_c_str("test.lpg");
        unicode_view const importing_file_content = unicode_view_from_c_str(source);
        complete_semantic_error const semantic_errors[] = {
            complete_semantic_error_create(
                semantic_error_create(semantic_error_unknown_element, source_location_create(0, 8)),
                unicode_view_from_string(imported_file), imported_file_content),
            complete_semantic_error_create(
                semantic_error_create(semantic_error_import_failed, source_location_create(0, 0)), importing_file,
                importing_file_content)};
        expected_semantic_errors expected = {semantic_errors, LPG_ARRAY_SIZE(semantic_errors)};
        expected_parse_errors const expected_parse_errors_ = {NULL, 0};
        checked_program checked =
            simple_check(root, std_library.globals, expect_errors, &expected, module_directory_view,
                         expected_parse_errors_, importing_file, importing_file_content);
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
        unicode_string_free(&imported_file);
    }

    {
        sequence root = parse("import doesnotexist\n");
        unicode_view const importing_file = unicode_view_from_c_str("");
        unicode_view const importing_file_content = unicode_view_from_c_str("");
        complete_semantic_error const semantic_errors[] = {complete_semantic_error_create(
            semantic_error_create(semantic_error_import_failed, source_location_create(0, 0)), importing_file,
            importing_file_content)};
        expected_semantic_errors expected = {semantic_errors, LPG_ARRAY_SIZE(semantic_errors)};
        expected_parse_errors const expected_parse_errors_ = {NULL, 0};
        checked_program checked =
            simple_check(root, std_library.globals, expect_errors, &expected, module_directory_view,
                         expected_parse_errors_, importing_file, importing_file_content);
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
    }

    {
        char const *const source = "import importsitself\n";
        sequence root = parse(source);
        unicode_view const imported_file_pieces[] = {
            module_directory_view, unicode_view_from_c_str("importsitself.lpg")};
        unicode_string const imported_file = path_combine(imported_file_pieces, LPG_ARRAY_SIZE(imported_file_pieces));
        unicode_view const imported_file_content = unicode_view_from_c_str("let self = import importsitself\n");
        unicode_view const importing_file = unicode_view_from_c_str("test.lpg");
        unicode_view const importing_file_content = unicode_view_from_c_str(source);
        complete_semantic_error const semantic_errors[] = {
            complete_semantic_error_create(
                semantic_error_create(semantic_error_import_failed, source_location_create(0, 11)),
                unicode_view_from_string(imported_file), imported_file_content),
            complete_semantic_error_create(
                semantic_error_create(semantic_error_import_failed, source_location_create(0, 0)), importing_file,
                importing_file_content)};
        expected_semantic_errors expected = {semantic_errors, LPG_ARRAY_SIZE(semantic_errors)};
        expected_parse_errors const expected_parse_errors_ = {NULL, 0};
        checked_program checked =
            simple_check(root, std_library.globals, expect_errors, &expected, module_directory_view,
                         expected_parse_errors_, importing_file, importing_file_content);
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
        unicode_string_free(&imported_file);
    }

    unicode_string_free(&module_directory);
    standard_library_description_free(&std_library);
}
