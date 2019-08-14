#include "lpg_load_module.h"
#include "lpg_allocate.h"
#include "lpg_array_size.h"
#include "lpg_check.h"
#include "lpg_find_next_token.h"
#include "lpg_interpret.h"
#include "lpg_parse_expression.h"
#include "lpg_path.h"
#include "lpg_read_file.h"

complete_parse_error complete_parse_error_create(parse_error relative, unicode_view file_name, unicode_view source,
                                                 source_file_lines lines)
{
    complete_parse_error const result = {relative, file_name, source, lines};
    return result;
}

bool complete_parse_error_equals(complete_parse_error const left, complete_parse_error const right)
{
    return parse_error_equals(left.relative, right.relative) && unicode_view_equals(left.file_name, right.file_name) &&
           unicode_view_equals(left.source, right.source) && source_file_lines_equals(left.lines, right.lines);
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
    source_file_lines lines;
    bool has_error;
} parse_error_translator;

static parse_error_translator parse_error_translator_create(complete_parse_error_handler *on_error,
                                                            callback_user on_error_user, unicode_view file_name,
                                                            unicode_view source, source_file_lines lines,
                                                            bool has_error)
{
    parse_error_translator const result = {on_error, on_error_user, file_name, source, lines, has_error};
    return result;
}

static void translate_parse_error(parse_error const error, void *user)
{
    parse_error_translator *const translator = user;
    complete_parse_error const complete_error =
        complete_parse_error_create(error, translator->file_name, translator->source, translator->lines);
    translator->has_error = true;
    translator->on_error(complete_error, translator->on_error_user);
}

typedef struct semantic_error_translator
{
    check_error_handler *on_error;
    void *user;
    bool has_error;
} semantic_error_translator;

static void translate_semantic_error(complete_semantic_error const error, void *user)
{
    semantic_error_translator *const translator = user;
    translator->has_error = true;
    translator->on_error(error, translator->user);
}

static load_module_result type_check_module(function_checking_state *const state, sequence const module_parsed,
                                            source_file_owning *const module_source,
                                            unicode_view const current_import_directory)
{
    semantic_error_translator translator = {state->on_error, state->user, false};
    check_function_result const checked = check_function(
        state->root, NULL, expression_from_sequence(module_parsed), state->root->global, translate_semantic_error,
        &translator, state->program, NULL, NULL, 0, optional_type_create_empty(), false, optional_type_create_empty(),
        module_source, current_import_directory, NULL, optional_function_id_empty());
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
    external_function_result const module_value =
        call_checked_function(checked.function, optional_function_id_empty(), NULL, optional_value_empty, NULL,
                              &state->root->compile_time_interpreter);
    switch (module_value.code)
    {
    case external_function_result_out_of_memory:
    case external_function_result_stack_overflow:
    case external_function_result_instruction_limit_reached:
        LPG_TO_DO();

    case external_function_result_success:
    {
        if (!checked.function.signature->result.is_set)
        {
            LPG_TO_DO();
        }
        load_module_result const success = {
            optional_value_create(module_value.if_success), checked.function.signature->result.value};
        checked_function_free(&checked.function);
        return success;
    }

    case external_function_result_unavailable:
    {
        checked_function_free(&checked.function);
        load_module_result const failure = {optional_value_empty, type_from_unit()};
        return failure;
    }
    }
    LPG_UNREACHABLE();
}

typedef struct import_attempt_result
{
    blob_or_error module_source;
    unicode_string full_module_path;
} import_attempt_result;

static void import_attempt_result_free(import_attempt_result const freed)
{
    unicode_string_free(&freed.full_module_path);
    if (!freed.module_source.error)
    {
        blob_free(&freed.module_source.success);
    }
}

static import_attempt_result try_load(unicode_view const module_directory, unicode_view name)
{
    unicode_string const file_name = unicode_view_concat(name, unicode_view_from_c_str(".lpg"));
    unicode_view const pieces[] = {module_directory, unicode_view_from_string(file_name)};
    unicode_string const full_module_path = path_combine(pieces, LPG_ARRAY_SIZE(pieces));
    unicode_string_free(&file_name);
    blob_or_error module_source = read_file_unicode_view_name(unicode_view_from_string(full_module_path));
    import_attempt_result const result = {module_source, full_module_path};
    return result;
}

load_module_result load_module(function_checking_state *const state, unicode_view const name)
{
    module_loader *const loader = state->root->loader;
    import_attempt_result attempt = try_load(loader->module_directory, name);
    if (attempt.module_source.error)
    {
        import_attempt_result_free(attempt);
        attempt = try_load(state->current_import_directory, name);
        if (attempt.module_source.error)
        {
            import_attempt_result_free(attempt);
            load_module_result const failure = {optional_value_empty, type_from_unit()};
            return failure;
        }
    }
    attempt.module_source.success.length =
        remove_carriage_returns(attempt.module_source.success.data, attempt.module_source.success.length);
    /*TODO: check whether source is UTF-8*/
    parser_user parser_state = {
        attempt.module_source.success.data, attempt.module_source.success.length, source_location_create(0, 0)};
    unicode_string const source_loaded = unicode_string_validate(attempt.module_source.success);
    source_file_lines_owning const lines = source_file_lines_owning_scan(unicode_view_from_string(source_loaded));
    parse_error_translator translator = parse_error_translator_create(
        loader->on_parse_error, loader->on_parse_error_user, unicode_view_from_string(attempt.full_module_path),
        unicode_view_create(attempt.module_source.success.data, attempt.module_source.success.length),
        source_file_lines_from_owning(lines), false);
    expression_pool pool = expression_pool_create();
    expression_parser parser = expression_parser_create(&parser_state, translate_parse_error, &translator, &pool);
    sequence const parsed = parse_program(&parser);
    expression_parser_free(parser);
    if (translator.has_error)
    {
        sequence_free(&parsed);
        expression_pool_free(pool);
        import_attempt_result_free(attempt);
        source_file_lines_owning_free(lines);
        load_module_result const failure = {optional_value_empty, type_from_unit()};
        return failure;
    }
    source_file_owning *module_source = allocate(sizeof(*module_source));
    *module_source = source_file_owning_create(attempt.full_module_path, source_loaded, lines);
    attempt.full_module_path.data = NULL;
    attempt.full_module_path.length = 0;
    attempt.module_source.success.data = NULL;
    attempt.module_source.success.length = 0;
    program_check_add_module_source(state->root, module_source);
    load_module_result const result = type_check_module(
        state, parsed, module_source, path_remove_leaf(unicode_view_from_string(module_source->name)));
    sequence_free(&parsed);
    expression_pool_free(pool);
    import_attempt_result_free(attempt);
    return result;
}
