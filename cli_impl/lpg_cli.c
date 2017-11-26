#ifndef _MSC_VER
#define _FILE_OFFSET_BITS 64
#endif
#include <stdio.h>
#include "lpg_cli.h"
#include "lpg_check.h"
#include "lpg_find_next_token.h"
#include "lpg_interpret.h"
#include "lpg_allocate.h"
#include "lpg_read_file.h"
#include "lpg_standard_library.h"

typedef struct cli_parser_user
{
    parser_user base;
    unicode_view source;
    stream_writer diagnostics;
    bool has_error;
} cli_parser_user;

static unicode_view find_whole_line(unicode_view const source, line_number const found_line)
{
    char const *begin_of_line = source.begin;
    char const *const end_of_file = source.begin + source.length;
    if (found_line > 0)
    {
        line_number current_line = 0;
        char const *i = begin_of_line;
        for (;;)
        {
            /*the location cannot be beyond the end of the file*/
            ASSUME(i != end_of_file);

            if (*i == '\r' || *i == '\n')
            {
                if (*i == '\r')
                {
                    char const *const newline = i + 1;
                    if ((newline != end_of_file) && (*newline == '\n'))
                    {
                        ++i;
                    }
                }
                ++i;
                ++current_line;
                if (current_line == found_line)
                {
                    begin_of_line = i;
                    break;
                }
            }
            else
            {
                ++i;
            }
        }
    }
    size_t line_length = 0;
    for (char const *i = begin_of_line; (i != end_of_file) && (*i != '\n') && (*i != '\r'); ++i)
    {
        ++line_length;
    }
    return unicode_view_create(begin_of_line, line_length);
}

static void print_source_location_hint(unicode_view const source, stream_writer const diagnostics,
                                       source_location const where)
{
    unicode_view const affected_line = find_whole_line(source, where.line);
    ASSERT(success == stream_writer_write_bytes(diagnostics, affected_line.begin, affected_line.length));
    ASSERT(success == stream_writer_write_string(diagnostics, "\n"));
    for (column_number i = 0; i < where.approximate_column; ++i)
    {
        ASSERT(success == stream_writer_write_string(diagnostics, " "));
    }
    ASSERT(success == stream_writer_write_string(diagnostics, "^\n"));
}

static char const *describe_parse_error(parse_error_type const error)
{
    switch (error)
    {
    case parse_error_invalid_token:
        return "Invalid token";
    case parse_error_expected_expression:
        return "Expected expression";
    case parse_error_expected_arguments:
        return "Expected arguments";
    case parse_error_integer_literal_out_of_range:
        return "Integer literal out of range";
    case parse_error_expected_newline:
        return "Expected new line";
    case parse_error_expected_assignment:
        return "Expected assignment";
    case parse_error_expected_declaration_or_assignment:
        return "Expected declaration or assignment";
    case parse_error_expected_space:
        return "Expected space";
    case parse_error_expected_colon:
        return "Expected colon";
    case parse_error_expected_case:
        return "Expected case";
    case parse_error_expected_element_name:
        return "Expected element name";
    case parse_error_expected_identifier:
        return "Expected identifier";
    case parse_error_expected_comma:
        return "Expected comma";
    case parse_error_expected_lambda_body:
        return "Expected lambda body";
    case parse_error_expected_return_type:
        return "Expected return type";
    case parse_error_unknown_binary_operator:
        return "Unknown binary operator";
    case parse_error_expected_parameter_list:
        return "Expected parameter list";
    case parse_error_expected_for:
        return "Expected context-sensitive keyword 'for'";
    }
    LPG_UNREACHABLE();
}

static void handle_parse_error(parse_error const error, callback_user const user)
{
    cli_parser_user *const actual_user = user;
    actual_user->has_error = true;
    ASSERT(success == stream_writer_write_string(actual_user->diagnostics, describe_parse_error(error.type)));
    ASSERT(success == stream_writer_write_string(actual_user->diagnostics, " in line "));
    char buffer[64];
    char const *const line =
        integer_format(integer_create(0, error.where.line + 1), lower_case_digits, 10, buffer, sizeof(buffer));
    ASSERT(line);
    ASSERT(success ==
           stream_writer_write_bytes(actual_user->diagnostics, line, (size_t)(buffer + sizeof(buffer) - line)));
    ASSERT(success == stream_writer_write_string(actual_user->diagnostics, ":\n"));
    print_source_location_hint(actual_user->source, actual_user->diagnostics, error.where);
}

typedef struct optional_sequence
{
    bool has_value;
    sequence value;
} optional_sequence;

static optional_sequence make_optional_sequence(sequence const value)
{
    optional_sequence const result = {true, value};
    return result;
}

static optional_sequence const optional_sequence_none = {false, {NULL, 0}};

static optional_sequence parse(unicode_view const input, stream_writer const diagnostics)
{
    cli_parser_user user = {{input.begin, input.length, source_location_create(0, 0)}, input, diagnostics, false};
    expression_parser parser = expression_parser_create(find_next_token, handle_parse_error, &user);
    sequence const result = parse_program(&parser);
    if (user.has_error)
    {
        sequence_free(&result);
        return optional_sequence_none;
    }
    ASSUME(user.base.remaining_size == 0);
    return make_optional_sequence(result);
}

typedef struct semantic_error_context
{
    unicode_view source;
    stream_writer diagnostics;
    bool has_error;
} semantic_error_context;

static char const *semantic_error_text(semantic_error_type const type)
{
    switch (type)
    {
    case semantic_error_unknown_element:
        return "Unknown structure element or global identifier";

    case semantic_error_expected_compile_time_type:
        return "Expected compile time type";

    case semantic_error_no_members_on_enum_elements:
        return "Instances of enums do not have members";

    case semantic_error_type_mismatch:
        return "Type mismatch";

    case semantic_error_missing_argument:
        return "Missing call argument";

    case semantic_error_extraneous_argument:
        return "Extraneous call argument";

    case semantic_error_break_outside_of_loop:
        return "Found break outside of a loop";

    case semantic_error_declaration_with_existing_name:
        return "Cannot reuse a name of a local variable in scope";

    case semantic_error_missing_match_case:
        return "At least one match case is missing";

    case semantic_error_duplicate_match_case:
        return "Match case already exists";

    case semantic_error_expected_interface:
        return "Expected interface";

    case semantic_error_duplicate_impl:
        return "Duplicate impl of an interface for the same self";

    case semantic_error_cannot_capture_runtime_variable:
        return "Cannot capture runtime variable in an impl method";

    case semantic_error_not_callable:
        return "This value cannot be called like a function";

    case semantic_error_duplicate_method_name:
        return "Duplicate method name in the same interface";
    }
    LPG_UNREACHABLE();
}

static void handle_semantic_error(semantic_error const error, void *user)
{
    semantic_error_context *const context = user;
    context->has_error = true;
    ASSERT(success == stream_writer_write_string(context->diagnostics, semantic_error_text(error.type)));
    ASSERT(success == stream_writer_write_string(context->diagnostics, " in line "));
    char buffer[64];
    char const *const line =
        integer_format(integer_create(0, error.where.line + 1), lower_case_digits, 10, buffer, sizeof(buffer));
    ASSERT(line);
    ASSERT(success == stream_writer_write_bytes(context->diagnostics, line, (size_t)(buffer + sizeof(buffer) - line)));
    ASSERT(success == stream_writer_write_string(context->diagnostics, ":\n"));
    print_source_location_hint(context->source, context->diagnostics, error.where);
}

static value print(function_call_arguments const arguments, struct value const *const captures, void *environment)
{
    (void)captures;
    unicode_view const text = arguments.arguments[0].string_ref;
    stream_writer *const destination = environment;
    ASSERT(stream_writer_write_bytes(*destination, text.begin, text.length) == success);
    return value_from_unit();
}

static compiler_flags parse_compiler_flags(int const argument_count, char **const arguments, bool *const valid)
{
    compiler_flags result = {false};
    for (int i = 2; i < argument_count; ++i)
    {
        unicode_view possible_flag = unicode_view_from_c_str(arguments[i]);
        if (unicode_view_equals(possible_flag, unicode_view_from_c_str("--compile-only")))
        {
            result.compile_only = true;
        }
        else
        {
            *valid = false;
        }
    }
    return result;
}

static compiler_arguments parse_compiler_arguments(int const argument_count, char **const arguments)
{
    compiler_arguments result = {false, "", {false}};
    if (argument_count < 2)
    {
        result.valid = false;
        return result;
    }

    bool valid = true;
    result.file_name = arguments[1];
    result.flags = parse_compiler_flags(argument_count, arguments, &valid);
    result.valid = valid;
    return result;
}

bool run_cli(int const argc, char **const argv, stream_writer const diagnostics, stream_writer print_destination)
{
    compiler_arguments arguments = parse_compiler_arguments(argc, argv);

    if (!arguments.valid)
    {
        ASSERT(success == stream_writer_write_string(diagnostics, "Arguments: filename\n"));
        return true;
    }

    blob_or_error const source_or_error = read_file(arguments.file_name);
    if (source_or_error.error)
    {
        ASSERT(success == stream_writer_write_string(diagnostics, source_or_error.error));
        return true;
    }

    unicode_string const source = unicode_string_validate(source_or_error.success);
    if (source.length != source_or_error.success.length)
    {
        ASSERT(success == stream_writer_write_string(diagnostics, "Source code must in in ASCII (UTF-8 is TODO)\n"));
        return true;
    }

    standard_library_description const standard_library = describe_standard_library();

    value globals_values[standard_library_element_count];
    ASSUME(LPG_ARRAY_SIZE(globals_values) == standard_library.globals.count);

    for (size_t i = 0; i < LPG_ARRAY_SIZE(globals_values); ++i)
    {
        globals_values[i] = value_from_unit();
    }
    globals_values[2] =
        value_from_function_pointer(function_pointer_value_from_external(print, &print_destination, NULL, 0));

    optional_sequence const root = parse(unicode_view_from_string(source), diagnostics);
    if (!root.has_value)
    {
        sequence_free(&root.value);
        standard_library_description_free(&standard_library);
        unicode_string_free(&source);
        return true;
    }
    semantic_error_context context = {unicode_view_from_string(source), diagnostics, false};
    checked_program checked = check(root.value, standard_library.globals, handle_semantic_error, &context);
    sequence_free(&root.value);
    if (!context.has_error && !arguments.flags.compile_only)
    {
        garbage_collector gc = {NULL};
        interpret(checked, globals_values, &gc);
        garbage_collector_free(gc);
    }
    checked_program_free(&checked);
    standard_library_description_free(&standard_library);
    unicode_string_free(&source);
    return context.has_error;
}
