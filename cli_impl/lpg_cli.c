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
#include "lpg_path.h"
#include "lpg_standard_library.h"
#include "lpg_save_expression.h"
#include "lpg_write_file.h"
#ifdef __linux__
#include <fcntl.h>
#include <errno.h>
#endif
#include <stdio.h>
#include "lpg_win32.h"
#include "lpg_rename_file.h"
#ifdef _WIN32
#include <Windows.h>
#endif

typedef struct cli_parser_user
{
    stream_writer diagnostics;
    bool has_error;
} cli_parser_user;

typedef struct parse_error_translator
{
    cli_parser_user *base;
    unicode_view file_name;
    unicode_view source;
} parse_error_translator;

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
    ASSERT(success_yes == stream_writer_write_bytes(diagnostics, affected_line.begin, affected_line.length));
    ASSERT(success_yes == stream_writer_write_string(diagnostics, "\n"));
    for (column_number i = 0; i < where.approximate_column; ++i)
    {
        ASSERT(success_yes == stream_writer_write_string(diagnostics, " "));
    }
    ASSERT(success_yes == stream_writer_write_string(diagnostics, "^\n"));
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
    case parse_error_expected_right_parenthesis:
        return "Expected right parenthesis";
    case parse_error_expected_left_parenthesis:
        return "Expected left parenthesis";
    case parse_error_expected_right_bracket:
        return "Expected right bracket";
    }
    LPG_UNREACHABLE();
}

static void handle_parse_error(complete_parse_error const error, callback_user const user)
{
    cli_parser_user *const actual_user = user;
    actual_user->has_error = true;
    ASSERT(success_yes ==
           stream_writer_write_string(actual_user->diagnostics, describe_parse_error(error.relative.type)));
    ASSERT(success_yes == stream_writer_write_string(actual_user->diagnostics, " in line "));
    char buffer[64];
    char const *const line =
        integer_format(integer_create(0, error.relative.where.line + 1), lower_case_digits, 10, buffer, sizeof(buffer));
    ASSERT(line);
    ASSERT(success_yes ==
           stream_writer_write_bytes(actual_user->diagnostics, line, (size_t)(buffer + sizeof(buffer) - line)));
    ASSERT(success_yes == stream_writer_write_string(actual_user->diagnostics, ":\n"));
    print_source_location_hint(error.source, actual_user->diagnostics, error.relative.where);
}

static void translate_parse_error(parse_error const error, callback_user const user)
{
    parse_error_translator *const actual_user = user;
    complete_parse_error const complete =
        complete_parse_error_create(error, actual_user->file_name, actual_user->source);
    handle_parse_error(complete, actual_user->base);
}

typedef struct optional_sequence
{
    bool has_value;
    sequence value;
} optional_sequence;

static optional_sequence make_optional_sequence(sequence const content)
{
    optional_sequence const result = {true, content};
    return result;
}

static optional_sequence const optional_sequence_none = {false, {NULL, 0}};

static optional_sequence parse(cli_parser_user user, unicode_view const file_name, unicode_view const source)
{
    parser_user parser_state = {source.begin, source.length, source_location_create(0, 0)};
    parse_error_translator translator = {&user, file_name, source};
    expression_parser parser =
        expression_parser_create(find_next_token, &parser_state, translate_parse_error, &translator);
    sequence const result = parse_program(&parser);
    if (user.has_error)
    {
        sequence_free(&result);
        return optional_sequence_none;
    }
    ASSUME(parser_state.remaining_size == 0);
    return make_optional_sequence(result);
}

typedef struct semantic_error_context
{
    stream_writer diagnostics;
    bool has_error;
} semantic_error_context;

static char const *semantic_error_text(semantic_error_type const error)
{
    switch (error)
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

    case semantic_error_expected_structure:
        return "Expected structure";

    case semantic_error_duplicate_enum_element:
        return "Duplicate enum element";

    case semantic_error_match_unsupported:
        return "Unsupported type to match";

    case semantic_error_expected_generic_type:
        return "Expected generic type";

    case semantic_error_expected_compile_time_value:
        return "Expected compile time value";

    case semantic_error_import_failed:
        return "import failed";
    }
    LPG_UNREACHABLE();
}

static void handle_semantic_error(complete_semantic_error const error, void *user)
{
    semantic_error_context *const context = user;
    context->has_error = true;
    ASSERT(success_yes == stream_writer_write_string(context->diagnostics, semantic_error_text(error.relative.type)));
    ASSERT(success_yes == stream_writer_write_string(context->diagnostics, " in line "));
    char buffer[64];
    char const *const line =
        integer_format(integer_create(0, error.relative.where.line + 1), lower_case_digits, 10, buffer, sizeof(buffer));
    ASSERT(line);
    ASSERT(success_yes ==
           stream_writer_write_bytes(context->diagnostics, line, (size_t)(buffer + sizeof(buffer) - line)));
    ASSERT(success_yes == stream_writer_write_string(context->diagnostics, ":\n"));
    print_source_location_hint(error.source, context->diagnostics, error.relative.where);
}

static compiler_flags parse_compiler_flags(int const argument_count, char **const arguments, bool *const valid)
{
    compiler_flags result = {false, false};
    for (int i = 2; i < argument_count; ++i)
    {
        unicode_view const possible_flag = unicode_view_from_c_str(arguments[i]);
        if (unicode_view_equals(possible_flag, unicode_view_from_c_str("--compile-only")))
        {
            result.compile_only = true;
        }
        else if (unicode_view_equals(possible_flag, unicode_view_from_c_str("--format")))
        {
            result.format = true;
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
    compiler_arguments result = {false, "", {false, false}};
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

bool run_cli(int const argc, char **const argv, stream_writer const diagnostics, unicode_view const module_directory)
{
    compiler_arguments arguments = parse_compiler_arguments(argc, argv);

    if (!arguments.valid)
    {
        ASSERT(success_yes == stream_writer_write_string(diagnostics, "Arguments: filename\n"));
        return true;
    }

    blob_or_error const source_or_error = read_file(arguments.file_name);
    if (source_or_error.error)
    {
        ASSERT(success_yes == stream_writer_write_string(diagnostics, source_or_error.error));
        return true;
    }

    unicode_string const source = unicode_string_validate(source_or_error.success);
    if (source.length != source_or_error.success.length)
    {
        ASSERT(success_yes ==
               stream_writer_write_string(diagnostics, "Source code must be in ASCII (UTF-8 is TODO)\n"));
        return true;
    }

    cli_parser_user user = {diagnostics, false};
    optional_sequence const root =
        parse(user, unicode_view_from_c_str(arguments.file_name), unicode_view_from_string(source));
    if (!root.has_value)
    {
        sequence_free(&root.value);
        unicode_string_free(&source);
        return true;
    }

    if (arguments.flags.format)
    {
        memory_writer format_buffer = {NULL, 0, 0};
        whitespace_state const whitespace = {0, false};
        if ((save_sequence(memory_writer_erase(&format_buffer), root.value, whitespace) != success_yes) ||
            (stream_writer_write_bytes(memory_writer_erase(&format_buffer), "", 1) != success_yes))
        {
            memory_writer_free(&format_buffer);
            sequence_free(&root.value);
            unicode_string_free(&source);
            return true;
        }

        sequence_free(&root.value);

        unicode_string temporary = write_temporary_file(format_buffer.data);
        if (!rename_file(unicode_view_from_string(temporary), unicode_view_from_c_str(arguments.file_name)))
        {
            ASSERT(success_yes == stream_writer_write_string(diagnostics, "Could not write formatted file\n"));
            memory_writer_free(&format_buffer);
            unicode_string_free(&temporary);
            unicode_string_free(&source);
            return true;
        }

        memory_writer_free(&format_buffer);
        unicode_string_free(&temporary);
        unicode_string_free(&source);
        return false;
    }

    standard_library_description const standard_library = describe_standard_library();
    value globals_values[standard_library_element_count];
    for (size_t i = 0; i < LPG_ARRAY_SIZE(globals_values); ++i)
    {
        globals_values[i] = value_from_unit();
    }
    ASSUME(LPG_ARRAY_SIZE(globals_values) == standard_library.globals.count);
    semantic_error_context context = {diagnostics, false};
    module_loader loader = module_loader_create(module_directory, handle_parse_error, &user);
    checked_program checked =
        check(root.value, standard_library.globals, handle_semantic_error, &loader,
              unicode_view_from_c_str(arguments.file_name), unicode_view_from_string(source), &context);
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
