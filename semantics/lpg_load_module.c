#include "lpg_load_module.h"
#include "lpg_path.h"
#include "lpg_array_size.h"
#include "lpg_read_file.h"
#include "lpg_parse_expression.h"
#include "lpg_find_next_token.h"
#include "lpg_check.h"
#include "lpg_interpret.h"

complete_parse_error complete_parse_error_create(parse_error relative, unicode_view file_name, unicode_view source)
{
    complete_parse_error const result = {relative, file_name, source};
    return result;
}

bool complete_parse_error_equals(complete_parse_error const left, complete_parse_error const right)
{
    return parse_error_equals(left.relative, right.relative) && unicode_view_equals(left.file_name, right.file_name) &&
           unicode_view_equals(left.source, right.source);
}

module_loader module_loader_create(unicode_view module_directory, complete_parse_error_handler on_parse_error,
                                   callback_user on_parse_error_user)
{
    module_loader result = {module_directory, on_parse_error, on_parse_error_user};
    return result;
}

typedef struct parse_error_translator
{
    complete_parse_error_handler *on_error;
    callback_user on_error_user;
    unicode_view file_name;
    unicode_view source;
    bool has_error;
} parse_error_translator;

static parse_error_translator parse_error_translator_create(complete_parse_error_handler *on_error,
                                                            callback_user on_error_user, unicode_view file_name,
                                                            unicode_view source, bool has_error)
{
    parse_error_translator const result = {on_error, on_error_user, file_name, source, has_error};
    return result;
}

static void translate_parse_error(parse_error const error, void *user)
{
    parse_error_translator *const translator = user;
    complete_parse_error const complete_error =
        complete_parse_error_create(error, translator->file_name, translator->source);
    translator->has_error = true;
    translator->on_error(complete_error, translator->on_error_user);
}

typedef struct semantic_error_translator
{
    check_error_handler *on_error;
    void *user;
    bool has_error;
} semantic_error_translator;

static void translate_semantic_error(semantic_error const error, void *user)
{
    semantic_error_translator *const translator = user;
    translator->has_error = true;
    translator->on_error(error, translator->user);
}

static load_module_result type_check_module(function_checking_state *state, sequence const parsed)
{
    semantic_error_translator translator = {state->on_error, state->user, false};
    check_function_result const checked = check_function(
        state->root, NULL, expression_from_sequence(parsed), state->root->global, translate_semantic_error, &translator,
        state->program, NULL, NULL, 0, optional_type_create_empty(), false, optional_type_create_empty());
    if (!checked.success)
    {
        load_module_result const failure = {optional_value_empty, type_from_unit()};
        return failure;
    }
    ASSERT(checked.capture_count == 0);
    if (translator.has_error)
    {
        checked_function_free(&checked.function);
        load_module_result const failure = {optional_value_empty, type_from_unit()};
        return failure;
    }
    optional_value const module_value = call_checked_function(
        checked.function, NULL,
        function_call_arguments_create(optional_value_empty, NULL, state->root->globals, &state->program->memory,
                                       state->program->functions, state->program->interfaces));
    if (!module_value.is_set)
    {
        checked_function_free(&checked.function);
        load_module_result const failure = {optional_value_empty, type_from_unit()};
        return failure;
    }
    load_module_result const success = {module_value, checked.function.signature->result};
    checked_function_free(&checked.function);
    return success;
}

static size_t remove_carriage_returns(char *const from, size_t const length)
{
    size_t new_length = 0;
    for (size_t i = 0; i < length; ++i)
    {
        if (from[i] == '\r')
        {
            continue;
        }
        from[new_length] = from[i];
        ++new_length;
    }
    return new_length;
}

load_module_result load_module(function_checking_state *state, unicode_view name)
{
    unicode_string const file_name = unicode_view_concat(name, unicode_view_from_c_str(".lpg"));
    module_loader *const loader = state->root->loader;
    unicode_view const pieces[] = {loader->module_directory, unicode_view_from_string(file_name)};
    unicode_string const full_module_path = path_combine(pieces, LPG_ARRAY_SIZE(pieces));
    unicode_string_free(&file_name);
    blob_or_error module_source = read_file_unicode_view_name(unicode_view_from_string(full_module_path));
    if (module_source.error)
    {
        unicode_string_free(&full_module_path);
        load_module_result const failure = {optional_value_empty, type_from_unit()};
        return failure;
    }
    module_source.success.length = remove_carriage_returns(module_source.success.data, module_source.success.length);
    /*TODO: check whether source is UTF-8*/
    parser_user parser_state = {module_source.success.data, module_source.success.length, source_location_create(0, 0)};
    parse_error_translator translator = parse_error_translator_create(
        loader->on_parse_error, loader->on_parse_error_user, unicode_view_from_string(full_module_path),
        unicode_view_create(module_source.success.data, module_source.success.length), false);
    expression_parser parser =
        expression_parser_create(find_next_token, &parser_state, translate_parse_error, &translator);
    sequence const parsed = parse_program(&parser);
    if (translator.has_error)
    {
        sequence_free(&parsed);
        unicode_string_free(&full_module_path);
        blob_free(&module_source.success);
        load_module_result const failure = {optional_value_empty, type_from_unit()};
        return failure;
    }
    load_module_result const result = type_check_module(state, parsed);
    sequence_free(&parsed);
    unicode_string_free(&full_module_path);
    blob_free(&module_source.success);
    return result;
}
