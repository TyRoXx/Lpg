#ifndef _MSC_VER
#define _FILE_OFFSET_BITS 64
#endif
#include "lpg_cli.h"
#include "lpg_allocate.h"
#include "lpg_check.h"
#include "lpg_ecmascript_backend.h"
#include "lpg_find_next_token.h"
#include "lpg_interpret.h"
#include "lpg_optimize.h"
#include "lpg_path.h"
#include "lpg_read_file.h"
#include "lpg_save_expression.h"
#include "lpg_standard_library.h"
#include "lpg_write_file.h"
#if LPG_WITH_NODEJS
#include "lpg_write_file_if_necessary.h"
#endif
#include <stdio.h>
#include <string.h>
#ifdef __linux__
#include <errno.h>
#include <fcntl.h>
#endif
#include "lpg_rename_file.h"
#include "lpg_win32.h"
#include "website.h"
#ifdef _WIN32
#include <Windows.h>
#endif

typedef struct parse_error_translator
{
    cli_parser_user *base;
    unicode_view file_name;
    unicode_view source;
    source_file_lines lines;
} parse_error_translator;

static unicode_view find_whole_line(unicode_view const source, source_file_lines const lines,
                                    line_number const found_line)
{
    ASSUME(found_line < lines.line_count);
    size_t const begin = lines.line_offsets[found_line];
    size_t const end =
        ((found_line + 1) == lines.line_count) ? source.length : (lines.line_offsets[found_line + 1] - 1u);
    return unicode_view_cut(source, begin, end);
}

static void print_source_location_hint(unicode_view const source, source_file_lines const lines,
                                       stream_writer const diagnostics, source_location const where)
{
    unicode_view const affected_line = find_whole_line(source, lines, where.line);
    ASSERT(success_yes == stream_writer_write_bytes(diagnostics, affected_line.begin, affected_line.length));
    ASSERT(success_yes == stream_writer_write_string(diagnostics, "\n"));
    char const *const bunch_of_spaces = "        ";
    for (column_number i = 0; i < (where.approximate_column / strlen(bunch_of_spaces)); ++i)
    {
        ASSERT(success_yes == stream_writer_write_string(diagnostics, bunch_of_spaces));
    }
    for (column_number i = 0; i < (where.approximate_column % strlen(bunch_of_spaces)); ++i)
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
    case parse_error_expected_declaration_or_assignment:
        return "Expected declaration or assignment";
    case parse_error_expected_space:
        return "Expected space";
    case parse_error_expected_colon:
        return "Expected colon";
    case parse_error_expected_element_name:
        return "Expected element name";
    case parse_error_expected_identifier:
        return "Expected identifier";
    case parse_error_expected_comma:
        return "Expected comma";
    case parse_error_expected_lambda_body:
        return "Expected lambda body";
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
    case parse_error_unexpected_indentation:
        return "Unexpected indentation";
    case parse_error_nesting_too_deep:
        return "Expressions are nested too deep";
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
    unicode_view const line =
        integer_format(integer_create(0, error.relative.where.line + 1), lower_case_digits, 10, buffer, sizeof(buffer));
    ASSERT(line.begin);
    ASSERT(success_yes == stream_writer_write_unicode_view(actual_user->diagnostics, line));
    ASSERT(success_yes == stream_writer_write_string(actual_user->diagnostics, ":\n"));
    print_source_location_hint(error.source, error.lines, actual_user->diagnostics, error.relative.where);
}

static void translate_parse_error(parse_error const error, callback_user const user)
{
    parse_error_translator *const actual_user = user;
    complete_parse_error const complete =
        complete_parse_error_create(error, actual_user->file_name, actual_user->source, actual_user->lines);
    handle_parse_error(complete, actual_user->base);
}

static optional_sequence make_optional_sequence(sequence const content)
{
    optional_sequence const result = {true, content};
    return result;
}

static optional_sequence const optional_sequence_none = {false, {NULL, 0, {0, 0}}};

optional_sequence parse(cli_parser_user user, unicode_view const file_name, unicode_view const source,
                        source_file_lines const lines, expression_pool *const pool)
{
    parser_user parser_state = {source.begin, source.length, source_location_create(0, 0)};
    parse_error_translator translator = {&user, file_name, source, lines};
    expression_parser parser = expression_parser_create(&parser_state, translate_parse_error, &translator, pool);
    sequence const result = parse_program(&parser);
    expression_parser_free(parser);
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

    case semantic_error_expression_recursion_limit_reached:
        return "Expression nesting limit exceeded";

    case semantic_error_missing_method:
        return "Missing method in impl";

    case semantic_error_extra_method:
        return "Extra method in impl";

    case semantic_error_compile_time_memory_limit_reached:
        return "Compile time memory limit reached";

    case semantic_error_stack_overflow:
        return "Call stack overflow at compile time";

    case semantic_error_instruction_limit_reached:
        return "Executed too many instructions at compile time";

    case semantic_error_missing_default:
        return "Default case is required for this type";

    case semantic_error_duplicate_default_case:
        return "A match must not have more than one default case";

    case semantic_error_generic_impl_parameter_mismatch:
        return "The generic parameters of the impl and the interface have to be exactly the same currently";

    case semantic_error_placeholder_not_supported_here:
        return "A placeholder cannot be used here";

    case semantic_error_unused_generic_parameter:
        return "Self and/or the interface have to use generic parameters";
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
    unicode_view const line =
        integer_format(integer_create(0, error.relative.where.line + 1), lower_case_digits, 10, buffer, sizeof(buffer));
    ASSERT(line.begin);
    ASSERT(success_yes == stream_writer_write_unicode_view(context->diagnostics, line));
    ASSERT(success_yes == stream_writer_write_string(context->diagnostics, ":\n"));
    print_source_location_hint(error.source.content, error.source.lines, context->diagnostics, error.relative.where);
}

static compiler_command parse_compiler_command(char const *const command, bool *const valid)
{
    if (!strcmp(command, "compile"))
    {
        return compiler_command_compile;
    }
    else if (!strcmp(command, "format"))
    {
        return compiler_command_format;
    }
    else if (!strcmp(command, "run"))
    {
        return compiler_command_run;
    }
    else if (!strcmp(command, "web"))
    {
        return compiler_command_web;
    }
#ifdef LPG_NODEJS
    else if (!strcmp(command, "node"))
    {
        return compiler_command_node;
    }
#endif
    else
    {
        *valid = false;
    }
    return compiler_command_compile;
}

static compiler_arguments parse_compiler_arguments(int const argument_count, char **const arguments)
{
    compiler_arguments result = {false, compiler_command_compile, "", NULL};
    if (argument_count < 3)
    {
        result.valid = false;
        return result;
    }

    bool valid = true;
    result.command = parse_compiler_command(arguments[1], &valid);
    result.file_name = arguments[2];
    result.valid = valid;
    switch (result.command)
    {
    case compiler_command_compile:
    case compiler_command_format:
    case compiler_command_run:
        if (argument_count != 3)
        {
            result.valid = false;
            return result;
        }
        break;

    case compiler_command_web:
        if (argument_count != 4)
        {
            result.valid = false;
            return result;
        }
        result.output_file_name = arguments[3];
        break;

#ifdef LPG_NODEJS
    case compiler_command_node:
        if (argument_count != 3)
        {
            result.valid = false;
            return result;
        }
        break;
#endif
    }
    return result;
}

bool run_cli(int const argc, char **const argv, stream_writer const diagnostics, unicode_view const current_directory,
             unicode_view const module_directory)
{
#ifndef LPG_NODEJS
    (void)current_directory;
#endif
    compiler_arguments arguments = parse_compiler_arguments(argc, argv);

    if (!arguments.valid)
    {
        ASSERT(success_yes == stream_writer_write_string(diagnostics, "Arguments: [run|format|compile|web] "
                                                                      "filename [web output file]\n"));
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

    source_file_lines_owning const lines = source_file_lines_owning_scan(unicode_view_from_string(source));
    cli_parser_user user = {diagnostics, false};
    expression_pool pool = expression_pool_create();
    optional_sequence const root = parse(user, unicode_view_from_c_str(arguments.file_name),
                                         unicode_view_from_string(source), source_file_lines_from_owning(lines), &pool);
    if (!root.has_value)
    {
        sequence_free(&root.value);
        expression_pool_free(pool);
        unicode_string_free(&source);
        source_file_lines_owning_free(lines);
        return true;
    }

    switch (arguments.command)
    {
    case compiler_command_format:
    {
        memory_writer format_buffer = {NULL, 0, 0};
        whitespace_state const whitespace = {0, false};
        if ((save_sequence(memory_writer_erase(&format_buffer), root.value, whitespace) != success_yes) ||
            (stream_writer_write_bytes(memory_writer_erase(&format_buffer), "", 1) != success_yes))
        {
            memory_writer_free(&format_buffer);
            sequence_free(&root.value);
            expression_pool_free(pool);
            unicode_string_free(&source);
            source_file_lines_owning_free(lines);
            return true;
        }

        sequence_free(&root.value);
        expression_pool_free(pool);

        unicode_string temporary = write_temporary_file(format_buffer.data);
        if (!rename_file(unicode_view_from_string(temporary), unicode_view_from_c_str(arguments.file_name)))
        {
            ASSERT(success_yes == stream_writer_write_string(diagnostics, "Could not write formatted file\n"));
            memory_writer_free(&format_buffer);
            unicode_string_free(&temporary);
            unicode_string_free(&source);
            source_file_lines_owning_free(lines);
            return true;
        }

        memory_writer_free(&format_buffer);
        unicode_string_free(&temporary);
        unicode_string_free(&source);
        source_file_lines_owning_free(lines);
        return false;
    }

    case compiler_command_compile:
    case compiler_command_run:
#ifdef LPG_NODEJS
    case compiler_command_node:
#endif
    case compiler_command_web:
        break;
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
    unicode_view const source_file_path = unicode_view_from_c_str(arguments.file_name);
    checked_program checked = check(
        root.value, standard_library.globals, handle_semantic_error, &loader,
        source_file_create(source_file_path, unicode_view_from_string(source), source_file_lines_from_owning(lines)),
        path_remove_leaf(source_file_path), 100000, &context);
    sequence_free(&root.value);
    expression_pool_free(pool);
    switch (arguments.command)
    {
    case compiler_command_run:
        if (!context.has_error)
        {
            garbage_collector gc = garbage_collector_create(SIZE_MAX);
            interpret(checked, globals_values, &gc);
            garbage_collector_free(gc);
        }
        break;

    case compiler_command_web:
        if (!context.has_error)
        {
            optimize(&checked);
            unicode_view const output_file_name = unicode_view_from_c_str(arguments.output_file_name);
            unicode_string const template = generate_template(diagnostics, arguments.file_name);
            generate_ecmascript_web_site(checked, unicode_view_from_string(template), output_file_name);
            unicode_string_free(&template);
        }
        break;

    case compiler_command_compile:
        break;

    case compiler_command_format:
        LPG_UNREACHABLE();

#ifdef LPG_NODEJS
    case compiler_command_node:
    {
        memory_writer generated = {NULL, 0, 0};
        if (success_yes != stream_writer_write_string(memory_writer_erase(&generated), "var main = "))
        {
            LPG_TO_DO();
        }
        {
            enum_encoding_strategy_cache strategy_cache =
                enum_encoding_strategy_cache_create(checked.enums, checked.enum_count);
            unicode_string const builtins = load_ecmascript_builtins();
            if (success_yes !=
                generate_ecmascript(checked, &strategy_cache, unicode_view_from_string(builtins), &generated))
            {
                LPG_TO_DO();
            }
            unicode_string_free(&builtins);
            if (success_yes != generate_host_class(&strategy_cache, get_host_interface(checked), checked.interfaces,
                                                   memory_writer_erase(&generated)))
            {
                LPG_TO_DO();
            }
            enum_encoding_strategy_cache_free(strategy_cache);
        }
        if (success_yes != stream_writer_write_string(
                               memory_writer_erase(&generated), "main(new Function('return this;')(), new Host());\n"))
        {
            LPG_TO_DO();
        }
        {
            unicode_view const pieces[] = {current_directory, unicode_view_from_c_str("generated.js")};
            unicode_string const generated_js = path_combine(pieces, LPG_ARRAY_SIZE(pieces));
            write_file_if_necessary(
                unicode_view_from_string(generated_js), unicode_view_create(generated.data, generated.used));
            unicode_string_free(&generated_js);
        }

        {
            unicode_view const node = unicode_view_from_c_str(LPG_NODEJS);
            if (success_yes != memory_writer_write(&generated, "", 1))
            {
                LPG_TO_DO();
            }
            unicode_string source_file_name = write_temporary_file(generated.data);
            unicode_view const argument = unicode_view_from_string(source_file_name);
            create_process_result const process_created =
                create_process(node, &argument, 1, current_directory, get_standard_input(), get_standard_output(),
                               get_standard_error());
            if (process_created.success != success_yes)
            {
                LPG_TO_DO();
            }
            if (0 != wait_for_process_exit(process_created.created))
            {
                LPG_TO_DO();
            }
            if (0 != remove(unicode_string_c_str(&source_file_name)))
            {
                LPG_TO_DO();
            }
            unicode_string_free(&source_file_name);
        }
        memory_writer_free(&generated);
        break;
    }
#endif
    }
    source_file_lines_owning_free(lines);
    checked_program_free(&checked);
    standard_library_description_free(&standard_library);
    unicode_string_free(&source);
    return context.has_error;
}
