#include "test_import_errors.h"
#include "find_test_module_directory.h"
#include "handle_parse_error.h"
#include "lpg_allocate.h"
#include "lpg_check.h"
#include "lpg_instruction.h"
#include "lpg_standard_library.h"
#include "test.h"
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
                                    expected_parse_errors expected_errors, source_file const source)
{
    module_loader loader = module_loader_create(module_directory, expect_no_complete_parse_error, &expected_errors);
    return check(root, global, on_error, &loader, source, module_directory, 100000, user);
}

void test_import_errors(void)
{
    unicode_string const module_directory = find_test_module_directory();
    unicode_view const module_directory_view = unicode_view_from_string(module_directory);
    standard_library_description const std_library = describe_standard_library();

    {
        expression_pool pool = expression_pool_create();
        sequence root = parse("import syntaxerror\n", &pool);
        unicode_view const importing_file = unicode_view_from_c_str("");
        unicode_view const importing_file_content = unicode_view_from_c_str("");
        source_file_lines_owning const importing_lines = source_file_lines_owning_scan(importing_file_content);
        complete_semantic_error const semantic_errors[] = {complete_semantic_error_create(
            semantic_error_create(semantic_error_import_failed, source_location_create(0, 0)),
            source_file_create(
                importing_file, importing_file_content, source_file_lines_from_owning(importing_lines)))};
        expected_semantic_errors expected = {semantic_errors, LPG_ARRAY_SIZE(semantic_errors)};
        unicode_view const expected_file_name_pieces[] = {
            module_directory_view, unicode_view_from_c_str("syntaxerror.lpg")};
        unicode_string const expected_file_name =
            path_combine(expected_file_name_pieces, LPG_ARRAY_SIZE(expected_file_name_pieces));
        unicode_view const expected_source = unicode_view_from_c_str("let a = .\n");
        source_file_lines_owning const expected_lines = source_file_lines_owning_scan(expected_source);
        complete_parse_error const parse_errors[] = {
            complete_parse_error_create(
                parse_error_create(parse_error_expected_expression, source_location_create(0, 8)),
                unicode_view_from_string(expected_file_name), expected_source,
                source_file_lines_from_owning(expected_lines)),
            complete_parse_error_create(
                parse_error_create(parse_error_expected_expression, source_location_create(0, 9)),
                unicode_view_from_string(expected_file_name), expected_source,
                source_file_lines_from_owning(expected_lines)),
            complete_parse_error_create(
                parse_error_create(parse_error_expected_expression, source_location_create(1, 0)),
                unicode_view_from_string(expected_file_name), expected_source,
                source_file_lines_from_owning(expected_lines))};
        expected_parse_errors const expected_parse_errors_ = {parse_errors, LPG_ARRAY_SIZE(parse_errors)};
        checked_program checked = simple_check(
            root, std_library.globals, expect_errors, &expected, module_directory_view, expected_parse_errors_,
            source_file_create(importing_file, importing_file_content, source_file_lines_from_owning(importing_lines)));
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
        unicode_string_free(&expected_file_name);
        source_file_lines_owning_free(importing_lines);
        source_file_lines_owning_free(expected_lines);
        expression_pool_free(pool);
    }

    {
        char const *const source = "import semanticerror\n";
        expression_pool pool = expression_pool_create();
        sequence root = parse(source, &pool);
        unicode_view const imported_file_pieces[] = {
            module_directory_view, unicode_view_from_c_str("semanticerror.lpg")};
        unicode_string const imported_file = path_combine(imported_file_pieces, LPG_ARRAY_SIZE(imported_file_pieces));
        unicode_view const imported_file_content = unicode_view_from_c_str("let a = unknown\n");
        unicode_view const importing_file = unicode_view_from_c_str("test.lpg");
        unicode_view const importing_file_content = unicode_view_from_c_str(source);
        source_file_lines_owning const imported_lines = source_file_lines_owning_scan(imported_file_content);
        source_file_lines_owning const importing_lines = source_file_lines_owning_scan(importing_file_content);
        complete_semantic_error const semantic_errors[] = {
            complete_semantic_error_create(
                semantic_error_create(semantic_error_unknown_element, source_location_create(0, 8)),
                source_file_create(unicode_view_from_string(imported_file), imported_file_content,
                                   source_file_lines_from_owning(imported_lines))),
            complete_semantic_error_create(
                semantic_error_create(semantic_error_import_failed, source_location_create(0, 0)),
                source_file_create(
                    importing_file, importing_file_content, source_file_lines_from_owning(importing_lines)))};
        expected_semantic_errors expected = {semantic_errors, LPG_ARRAY_SIZE(semantic_errors)};
        expected_parse_errors const expected_parse_errors_ = {NULL, 0};
        checked_program checked = simple_check(
            root, std_library.globals, expect_errors, &expected, module_directory_view, expected_parse_errors_,
            source_file_create(importing_file, importing_file_content, source_file_lines_from_owning(importing_lines)));
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
        unicode_string_free(&imported_file);
        source_file_lines_owning_free(imported_lines);
        source_file_lines_owning_free(importing_lines);
        expression_pool_free(pool);
    }

    {
        expression_pool pool = expression_pool_create();
        sequence root = parse("import doesnotexist\n", &pool);
        unicode_view const importing_file = unicode_view_from_c_str("");
        unicode_view const importing_file_content = unicode_view_from_c_str("");
        source_file_lines_owning const lines = source_file_lines_owning_scan(importing_file_content);
        complete_semantic_error const semantic_errors[] = {complete_semantic_error_create(
            semantic_error_create(semantic_error_import_failed, source_location_create(0, 0)),
            source_file_create(importing_file, importing_file_content, source_file_lines_from_owning(lines)))};
        expected_semantic_errors expected = {semantic_errors, LPG_ARRAY_SIZE(semantic_errors)};
        expected_parse_errors const expected_parse_errors_ = {NULL, 0};
        checked_program checked = simple_check(
            root, std_library.globals, expect_errors, &expected, module_directory_view, expected_parse_errors_,
            source_file_create(importing_file, importing_file_content, source_file_lines_from_owning(lines)));
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
        source_file_lines_owning_free(lines);
        expression_pool_free(pool);
    }

    {
        char const *const source = "import importsitself\n";
        expression_pool pool = expression_pool_create();
        sequence root = parse(source, &pool);
        unicode_view const imported_file_pieces[] = {
            module_directory_view, unicode_view_from_c_str("importsitself.lpg")};
        unicode_string const imported_file = path_combine(imported_file_pieces, LPG_ARRAY_SIZE(imported_file_pieces));
        unicode_view const imported_file_content = unicode_view_from_c_str("let self = import importsitself\n");
        unicode_view const importing_file = unicode_view_from_c_str("test.lpg");
        unicode_view const importing_file_content = unicode_view_from_c_str(source);
        source_file_lines_owning const importing_lines = source_file_lines_owning_scan(importing_file_content);
        source_file_lines_owning const imported_lines = source_file_lines_owning_scan(imported_file_content);
        complete_semantic_error const semantic_errors[] = {
            complete_semantic_error_create(
                semantic_error_create(semantic_error_import_failed, source_location_create(0, 11)),
                source_file_create(unicode_view_from_string(imported_file), imported_file_content,
                                   source_file_lines_from_owning(imported_lines))),
            complete_semantic_error_create(
                semantic_error_create(semantic_error_import_failed, source_location_create(0, 0)),
                source_file_create(
                    importing_file, importing_file_content, source_file_lines_from_owning(importing_lines)))};
        expected_semantic_errors expected = {semantic_errors, LPG_ARRAY_SIZE(semantic_errors)};
        expected_parse_errors const expected_parse_errors_ = {NULL, 0};
        checked_program checked = simple_check(
            root, std_library.globals, expect_errors, &expected, module_directory_view, expected_parse_errors_,
            source_file_create(importing_file, importing_file_content, source_file_lines_from_owning(importing_lines)));
        REQUIRE(expected.count == 0);
        sequence_free(&root);
        REQUIRE(checked.function_count == 1);
        checked_program_free(&checked);
        unicode_string_free(&imported_file);
        source_file_lines_owning_free(importing_lines);
        source_file_lines_owning_free(imported_lines);
        expression_pool_free(pool);
    }

    unicode_string_free(&module_directory);
    standard_library_description_free(&std_library);
}
