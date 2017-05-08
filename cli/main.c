#ifndef _MSC_VER
#define _FILE_OFFSET_BITS 64
#endif
#include <stdio.h>
#include "lpg_find_next_token.h"
#include "lpg_stream_writer.h"
#include "lpg_check.h"
#include "lpg_interprete.h"
#include <string.h>
#include "lpg_assert.h"
#include "lpg_allocate.h"
#if LPG_WITH_VLD
#include <vld.h>
#endif

static success_indicator write_file(void *const user, char const *const data,
                                    size_t const length)
{
    if (fwrite(data, 1, length, user) == length)
    {
        return success;
    }
    return failure;
}

static stream_writer make_file_writer(FILE *const file)
{
    stream_writer const result = {write_file, file};
    return result;
}

typedef struct cli_parser_user
{
    parser_user base;
    unicode_view source;
    stream_writer diagnostics;
    bool has_error;
} cli_parser_user;

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
    }
    UNREACHABLE();
}

static void handle_parse_error(parse_error const error,
                               callback_user const user)
{
    cli_parser_user *const actual_user = user;
    actual_user->has_error = true;
    ASSERT(success ==
           stream_writer_write_string(
               actual_user->diagnostics, describe_parse_error(error.type)));
    ASSERT(success ==
           stream_writer_write_string(actual_user->diagnostics, " in line "));
    char buffer[64];
    char const *const line =
        integer_format(integer_create(0, error.where.line + 1),
                       lower_case_digits, 10, buffer, sizeof(buffer));
    ASSERT(success ==
           stream_writer_write_bytes(actual_user->diagnostics, line,
                                     (size_t)(buffer + sizeof(buffer) - line)));
    ASSERT(success ==
           stream_writer_write_string(actual_user->diagnostics, "\n"));
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

static optional_sequence parse(unicode_view const input,
                               stream_writer const diagnostics)
{
    cli_parser_user user = {
        {input.begin, input.length, source_location_create(0, 0)},
        input,
        diagnostics,
        false};
    expression_parser parser =
        expression_parser_create(find_next_token, handle_parse_error, &user);
    sequence const result = parse_program(&parser);
    if (user.has_error)
    {
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

static void handle_semantic_error(semantic_error const error, void *user)
{
    semantic_error_context *const context = user;
    context->has_error = true;
    switch (error.type)
    {
    case semantic_error_unknown_identifier:
        ASSERT(success == stream_writer_write_string(
                              context->diagnostics, "Unknown identifier"));
        ASSERT(success ==
               stream_writer_write_string(context->diagnostics, " in line "));
        char buffer[64];
        char const *const line =
            integer_format(integer_create(0, error.where.line + 1),
                           lower_case_digits, 10, buffer, sizeof(buffer));
        ASSERT(success == stream_writer_write_bytes(
                              context->diagnostics, line,
                              (size_t)(buffer + sizeof(buffer) - line)));
        ASSERT(success ==
               stream_writer_write_string(context->diagnostics, "\n"));
        break;
    }
}

static value print(value const *arguments, void *environment)
{
    unicode_view const text = arguments[0].string_ref;
    stream_writer *const destination = environment;
    stream_writer_write_bytes(*destination, text.begin, text.length);
    return value_from_unit();
}

int main(int const argc, char **const argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "Arguments: filename\n");
        return 1;
    }

#ifdef _WIN32
    FILE *source_file = NULL;
    fopen_s(&source_file, argv[1], "rb");
#else
    FILE *const source_file = fopen(argv[1], "rb");
#endif
    if (!source_file)
    {
        fprintf(stderr, "Could not open source file\n");
        return 1;
    }

    fseek(source_file, 0, SEEK_END);

#ifdef _WIN32
    long long const source_size = _ftelli64
#else
    off_t const source_size = ftello
#endif
        (source_file);
    if (source_size < 0)
    {
        fprintf(stderr, "Could not determine size of source file\n");
        fclose(source_file);
        return 1;
    }

#if (SIZE_MAX < UINT64_MAX)
    if (source_size > SIZE_MAX)
    {
        fprintf(stderr, "Source file does not fit into memory\n");
        fclose(source_file);
        return 1;
    }
#endif

    fseek(source_file, 0, SEEK_SET);
    size_t const checked_source_size = (size_t)source_size;
    unicode_string const source = {
        allocate(checked_source_size), checked_source_size};
    size_t const read_result =
        fread(source.data, 1, source.length, source_file);
    if (read_result != source.length)
    {
        fprintf(stderr, "Could not read from source file\n");
        fclose(source_file);
        unicode_string_free(&source);
        return 1;
    }

    fclose(source_file);

    structure_member *const globals = allocate_array(1, sizeof(*globals));
    globals[0] = structure_member_create(
        type_from_function_pointer(
            function_pointer_create(type_allocate(type_from_unit()),
                                    type_allocate(type_from_string_ref()), 1)),
        unicode_string_from_c_str("print"));
    structure const global_object = structure_create(globals, 1);

    stream_writer print_destination = make_file_writer(stdout);
    value const globals_values[1] = {value_from_function_pointer(
        function_pointer_value_from_external(print, &print_destination))};
    stream_writer const diagnostics = make_file_writer(stderr);
    optional_sequence const root =
        parse(unicode_view_from_string(source), diagnostics);
    if (!root.has_value)
    {
        sequence_free(&root.value);
        structure_free(&global_object);
        unicode_string_free(&source);
        return 1;
    }
    semantic_error_context context = {
        unicode_view_from_string(source), diagnostics, false};
    checked_program checked =
        check(root.value, global_object, handle_semantic_error, &context);
    sequence_free(&root.value);
    interprete(checked, globals_values);
    checked_program_free(&checked);
    structure_free(&global_object);
    unicode_string_free(&source);
    if (context.has_error)
    {
        return 1;
    }
    return 0;
}
