#include "lpg_c_backend.h"
#include "lpg_assert.h"
#include "lpg_instruction.h"
#include "lpg_allocate.h"
#include <string.h>
#include <lpg_instruction.h>

typedef struct standard_library_usage
{
    bool using_string_ref;
    bool using_read;
    bool using_stdio;
    bool using_assert;
    bool using_unit;
    bool using_stdint;
} standard_library_usage;

static void standard_library_usage_use_string_ref(standard_library_usage *usage)
{
    usage->using_string_ref = true;
}

typedef enum register_meaning
{
    register_meaning_nothing,
    register_meaning_global,
    register_meaning_variable,
    register_meaning_print,
    register_meaning_assert,
    register_meaning_string_equals,
    register_meaning_read,
    register_meaning_or,
    register_meaning_and,
    register_meaning_not,
    register_meaning_concat,
    register_meaning_literal,
    register_meaning_argument
} register_meaning;

typedef enum register_resource_ownership
{
    register_resource_ownership_none,
    register_resource_ownership_owns_string,
    register_resource_ownership_borrows_string
} register_resource_ownership;

typedef struct register_function
{
    function_id id;
    type result;
} register_function;

typedef struct register_state
{
    register_meaning meaning;
    register_resource_ownership ownership;
    union
    {
        value literal;
        register_function function;
        register_id argument;
    };
} register_state;

typedef struct type_definition
{
    type what;
    unicode_string name;
    unicode_string definition;
} type_definition;

static void type_definition_free(type_definition const definition)
{

    unicode_string_free(&definition.definition);
    unicode_string_free(&definition.name);
}

typedef struct type_definitions
{
    type_definition *elements;
    size_t count;
} type_definitions;

static void type_definitions_free(type_definitions const definitions)
{
    for (size_t i = 0; i < definitions.count; ++i)
    {
        type_definition_free(definitions.elements[i]);
    }
    if (definitions.elements)
    {
        deallocate(definitions.elements);
    }
}

typedef struct c_backend_state
{
    register_state *registers;
    standard_library_usage standard_library;
    register_id *active_registers;
    size_t active_register_count;
    checked_function const *all_functions;
    type_definitions *definitions;
} c_backend_state;

static void active_register(c_backend_state *const state, register_id const id)
{
    for (size_t i = 0; i < state->active_register_count; ++i)
    {
        ASSERT(state->active_registers[i] != id);
    }
    state->active_registers = reallocate_array(
        state->active_registers, (state->active_register_count + 1),
        sizeof(*state->active_registers));
    state->active_registers[state->active_register_count] = id;
    ++(state->active_register_count);
}

static void set_register_meaning(c_backend_state *const state,
                                 register_id const id,
                                 register_meaning const meaning)
{
    ASSERT(state->registers[id].meaning == register_meaning_nothing);
    ASSERT(meaning != register_meaning_nothing);
    ASSERT(meaning != register_meaning_variable);
    ASSERT(meaning != register_meaning_literal);
    state->registers[id].meaning = meaning;
    active_register(state, id);
}

static register_resource_ownership
find_register_resource_ownership(type const variable)
{
    switch (variable.kind)
    {
    case type_kind_enumeration:
        LPG_TO_DO();

    case type_kind_tuple:
    case type_kind_function_pointer:
        return register_resource_ownership_none;

    case type_kind_inferred:
    case type_kind_integer_range:
    case type_kind_structure:
    case type_kind_type:
        LPG_TO_DO();

    case type_kind_string_ref:
        return register_resource_ownership_owns_string;

    case type_kind_unit:
        return register_resource_ownership_none;

    case type_kind_enum_constructor:
        LPG_TO_DO();
    }
    LPG_UNREACHABLE();
}

static void set_register_variable(c_backend_state *const state,
                                  register_id const id,
                                  register_resource_ownership ownership)
{
    ASSERT(state->registers[id].meaning == register_meaning_nothing);
    state->registers[id].meaning = register_meaning_variable;
    state->registers[id].ownership = ownership;
    active_register(state, id);
}

static void set_register_literal(c_backend_state *const state,
                                 register_id const id, value const literal)
{
    ASSERT(state->registers[id].meaning == register_meaning_nothing);
    state->registers[id].meaning = register_meaning_literal;
    state->registers[id].literal = literal;
    active_register(state, id);
}

static void set_register_argument(c_backend_state *const state,
                                  register_id const id,
                                  register_id const argument,
                                  register_resource_ownership const ownership)
{
    ASSERT(state->registers[id].meaning == register_meaning_nothing);
    state->registers[id].meaning = register_meaning_argument;
    state->registers[id].argument = argument;
    state->registers[id].ownership = ownership;
    active_register(state, id);
}

static success_indicator encode_string_literal(unicode_view const content,
                                               stream_writer const c_output)
{
    LPG_TRY(stream_writer_write_string(c_output, "\""));
    for (size_t i = 0; i < content.length; ++i)
    {
        switch (content.begin[i])
        {
        case '\n':
            LPG_TRY(stream_writer_write_string(c_output, "\\n"));
            break;

        default:
            LPG_TRY(stream_writer_write_bytes(c_output, content.begin + i, 1));
        }
    }
    LPG_TRY(stream_writer_write_string(c_output, "\""));
    return success;
}

static success_indicator generate_register_name(register_id const id,
                                                stream_writer const c_output)
{
    LPG_TRY(stream_writer_write_string(c_output, "r_"));
    char buffer[64];
    char const *const formatted = integer_format(
        integer_create(0, id), lower_case_digits, 10, buffer, sizeof(buffer));
    LPG_TRY(stream_writer_write_bytes(
        c_output, formatted, (size_t)((buffer + sizeof(buffer)) - formatted)));
    return success;
}

static success_indicator generate_function_name(function_id const id,
                                                stream_writer const c_output)
{
    LPG_TRY(stream_writer_write_string(c_output, "lambda_"));
    char buffer[64];
    char const *const formatted = integer_format(
        integer_create(0, id), lower_case_digits, 10, buffer, sizeof(buffer));
    LPG_TRY(stream_writer_write_bytes(
        c_output, formatted, (size_t)((buffer + sizeof(buffer)) - formatted)));
    return success;
}

static unicode_string const *
find_type_definition(type_definitions const definitions, type const needle)
{
    for (size_t i = 0; i < definitions.count; ++i)
    {
        if (type_equals(definitions.elements[i].what, needle))
        {
            return &definitions.elements[i].name;
        }
    }
    return NULL;
}

static unicode_string make_type_definition_name(size_t const index)
{
    char buffer[64];
    char *const formatted =
        integer_format(integer_create(0, index), lower_case_digits, 10, buffer,
                       sizeof(buffer));
    size_t const index_length = (size_t)((buffer + sizeof(buffer)) - formatted);
    char const *const prefix = "type_definition_";
    size_t const name_length = strlen(prefix) + index_length;
    unicode_string name = {allocate(name_length), name_length};
    memcpy(name.data, prefix, strlen(prefix));
    memcpy(name.data + strlen(prefix), formatted, index_length);
    return name;
}

static success_indicator
generate_type(type const generated,
              standard_library_usage *const standard_library,
              type_definitions *const definitions, stream_writer const c_output)
{
    switch (generated.kind)
    {
    case type_kind_structure:
        LPG_TO_DO();

    case type_kind_function_pointer:
    {
        unicode_string const *const existing_definition =
            find_type_definition(*definitions, generated);
        if (existing_definition)
        {
            return stream_writer_write_unicode_view(
                c_output, unicode_view_from_string(*existing_definition));
        }
        else
        {
            memory_writer definition_buffer = {NULL, 0, 0};
            stream_writer definition_writer =
                memory_writer_erase(&definition_buffer);
            size_t const definition_index = definitions->count;
            ++(definitions->count);
            unicode_string name = make_type_definition_name(definition_index);
            definitions->elements = reallocate_array(
                definitions->elements, (definitions->count + 1),
                sizeof(*definitions->elements));
            {
                type_definition *const new_definition =
                    definitions->elements + definition_index;
                new_definition->what = generated;
                new_definition->name = name;
            }
            LPG_TRY(stream_writer_write_string(definition_writer, "typedef "));
            LPG_TRY(generate_type(generated.function_pointer_->result,
                                  standard_library, definitions,
                                  definition_writer));
            LPG_TRY(stream_writer_write_string(definition_writer, " (*"));
            LPG_TRY(stream_writer_write_unicode_view(
                definition_writer, unicode_view_from_string(name)));
            LPG_TRY(stream_writer_write_string(definition_writer, ")("));
            for (size_t i = 0; i < generated.function_pointer_->arity; ++i)
            {
                if (i > 0)
                {
                    LPG_TRY(
                        stream_writer_write_string(definition_writer, ", "));
                }
                LPG_TRY(generate_type(generated.function_pointer_->arguments[i],
                                      standard_library, definitions,
                                      definition_writer));
            }
            LPG_TRY(stream_writer_write_string(definition_writer, ");\n"));
            type_definition *const new_definition =
                definitions->elements + definition_index;
            new_definition->definition.data = definition_buffer.data;
            new_definition->definition.length = definition_buffer.used;
            return stream_writer_write_unicode_view(
                c_output, unicode_view_from_string(name));
        }
    }

    case type_kind_unit:
        return stream_writer_write_string(c_output, "unit");

    case type_kind_string_ref:
        return stream_writer_write_string(c_output, "string_ref");

    case type_kind_enumeration:
        return stream_writer_write_string(c_output, "size_t");

    case type_kind_tuple:
    case type_kind_type:
        LPG_TO_DO();

    case type_kind_integer_range:
        if (integer_less(
                generated.integer_range_.maximum, integer_create(1, 0)))
        {
            standard_library->using_stdint = true;
            return stream_writer_write_string(c_output, "uint64_t");
        }
        LPG_TO_DO();

    case type_kind_inferred:
        LPG_TO_DO();

    case type_kind_enum_constructor:
        LPG_TO_DO();
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_parameter_name(register_id const argument,
                                                 stream_writer const c_output)
{
    LPG_TRY(stream_writer_write_string(c_output, "arg"));
    char buffer[64];
    char const *const formatted =
        integer_format(integer_create(0, argument), lower_case_digits, 10,
                       buffer, sizeof(buffer));
    return stream_writer_write_bytes(
        c_output, formatted, (size_t)((buffer + sizeof(buffer)) - formatted));
}

static success_indicator generate_c_read_access(c_backend_state *state,
                                                register_id const from,
                                                stream_writer const c_output)
{
    switch (state->registers[from].meaning)
    {
    case register_meaning_nothing:
        LPG_UNREACHABLE();

    case register_meaning_global:
        LPG_TO_DO();

    case register_meaning_variable:
        return generate_register_name(from, c_output);

    case register_meaning_print:
        LPG_TO_DO();

    case register_meaning_read:
        LPG_TO_DO();

    case register_meaning_assert:
        LPG_TO_DO();

    case register_meaning_string_equals:
        LPG_TO_DO();

    case register_meaning_concat:
        LPG_TO_DO();

    case register_meaning_literal:
        switch (state->registers[from].literal.kind)
        {
        case value_kind_integer:
        case value_kind_tuple:
            LPG_TO_DO();

        case value_kind_string:
            state->standard_library.using_string_ref = true;
            LPG_TRY(stream_writer_write_string(c_output, "string_literal("));
            LPG_TRY(encode_string_literal(
                state->registers[from].literal.string_ref, c_output));
            LPG_TRY(stream_writer_write_string(c_output, ", "));
            {
                char buffer[40];
                char *formatted = integer_format(
                    integer_create(
                        0, state->registers[from].literal.string_ref.length),
                    lower_case_digits, 10, buffer, sizeof(buffer));
                LPG_TRY(stream_writer_write_bytes(
                    c_output, formatted,
                    (size_t)((buffer + sizeof(buffer)) - formatted)));
            }
            LPG_TRY(stream_writer_write_string(c_output, ")"));
            return success;

        case value_kind_function_pointer:
            if (state->registers[from].literal.function_pointer.external)
            {
                LPG_TO_DO();
            }
            return generate_function_name(
                state->registers[from].literal.function_pointer.code, c_output);

        case value_kind_flat_object:
        case value_kind_type:
            LPG_TO_DO();

        case value_kind_enum_element:
        {
            char buffer[64];
            char const *const formatted = integer_format(
                integer_create(
                    0, state->registers[from].literal.enum_element.which),
                lower_case_digits, 10, buffer, sizeof(buffer));
            return stream_writer_write_bytes(
                c_output, formatted,
                (size_t)((buffer + sizeof(buffer)) - formatted));
        }

        case value_kind_unit:
            return stream_writer_write_string(c_output, "unit_impl");

        case value_kind_enum_constructor:
            LPG_TO_DO();
        }

    case register_meaning_argument:
        return generate_parameter_name(
            state->registers[from].argument, c_output);

    case register_meaning_and:
    case register_meaning_not:
    case register_meaning_or:
        LPG_TO_DO();
    }
    LPG_UNREACHABLE();
}

static success_indicator
generate_add_reference_for_return_value(c_backend_state *state,
                                        register_id const from,
                                        stream_writer const c_output)
{
    switch (state->registers[from].meaning)
    {
    case register_meaning_nothing:
        LPG_TO_DO();

    case register_meaning_global:
        LPG_TO_DO();

    case register_meaning_variable:
    case register_meaning_argument:
        switch (state->registers[from].ownership)
        {
        case register_resource_ownership_none:
            return success;

        case register_resource_ownership_borrows_string:
            LPG_TRY(stream_writer_write_string(
                c_output, "    string_ref_add_reference(&"));
            LPG_TRY(generate_c_read_access(state, from, c_output));
            return stream_writer_write_string(c_output, ");\n");

        case register_resource_ownership_owns_string:
            return success;
        }

    case register_meaning_print:
        LPG_TO_DO();

    case register_meaning_read:
        LPG_TO_DO();

    case register_meaning_assert:
        LPG_TO_DO();

    case register_meaning_string_equals:
        LPG_TO_DO();

    case register_meaning_concat:
        LPG_TO_DO();

    case register_meaning_literal:
        return success;

    case register_meaning_and:
    case register_meaning_not:
    case register_meaning_or:
        LPG_TO_DO();
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_c_str(c_backend_state *state,
                                        register_id const from,
                                        stream_writer const c_output)
{
    switch (state->registers[from].meaning)
    {
    case register_meaning_nothing:
    case register_meaning_global:
        LPG_UNREACHABLE();

    case register_meaning_variable:
    case register_meaning_argument:
        LPG_TRY(generate_c_read_access(state, from, c_output));
        return stream_writer_write_string(c_output, ".data");

    case register_meaning_print:
    case register_meaning_read:
    case register_meaning_assert:
    case register_meaning_string_equals:
    case register_meaning_concat:
        LPG_UNREACHABLE();

    case register_meaning_literal:
        switch (state->registers[from].literal.kind)
        {
        case value_kind_integer:
            LPG_UNREACHABLE();

        case value_kind_string:
            return encode_string_literal(
                state->registers[from].literal.string_ref, c_output);

        case value_kind_function_pointer:
        case value_kind_flat_object:
        case value_kind_type:
        case value_kind_enum_element:
        case value_kind_unit:
        case value_kind_tuple:
        case value_kind_enum_constructor:
            LPG_UNREACHABLE();
        }

    case register_meaning_and:
    case register_meaning_not:
    case register_meaning_or:
        LPG_UNREACHABLE();
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_string_length(c_backend_state *state,
                                                register_id const from,
                                                stream_writer const c_output)
{
    switch (state->registers[from].meaning)
    {
    case register_meaning_nothing:
    case register_meaning_global:
        LPG_UNREACHABLE();

    case register_meaning_argument:
    case register_meaning_variable:
        LPG_TRY(generate_c_read_access(state, from, c_output));
        return stream_writer_write_string(c_output, ".length");

    case register_meaning_print:
    case register_meaning_read:
    case register_meaning_assert:
    case register_meaning_string_equals:
    case register_meaning_concat:
        LPG_UNREACHABLE();

    case register_meaning_and:
    case register_meaning_not:
    case register_meaning_or:
        LPG_UNREACHABLE();

    case register_meaning_literal:
        switch (state->registers[from].literal.kind)
        {
        case value_kind_integer:
        case value_kind_tuple:
            LPG_UNREACHABLE();

        case value_kind_string:
        {
            char buffer[40];
            char *formatted = integer_format(
                integer_create(
                    0, state->registers[from].literal.string_ref.length),
                lower_case_digits, 10, buffer, sizeof(buffer));
            return stream_writer_write_bytes(
                c_output, formatted,
                (size_t)((buffer + sizeof(buffer)) - formatted));
        }

        case value_kind_function_pointer:
        case value_kind_flat_object:
        case value_kind_type:
        case value_kind_enum_element:
        case value_kind_unit:
        case value_kind_enum_constructor:
            LPG_UNREACHABLE();
        }
        LPG_UNREACHABLE();
    }
    LPG_UNREACHABLE();
}

static success_indicator
generate_sequence(c_backend_state *state,
                  checked_function const *const all_functions,
                  register_id const current_function_result,
                  instruction_sequence const sequence, size_t const indentation,
                  stream_writer const c_output);

static success_indicator indent(size_t const indentation,
                                stream_writer const c_output)
{
    for (size_t i = 0; i < indentation; ++i)
    {
        LPG_TRY(stream_writer_write_string(c_output, "    "));
    }
    return success;
}

static function_pointer signature_of(LPG_NON_NULL(c_backend_state *const state),
                                     function_pointer_value const pointer)
{
    if (pointer.external)
    {
        LPG_TO_DO();
    }
    return *state->all_functions[pointer.code].signature;
}

static success_indicator generate_instruction(
    c_backend_state *state, checked_function const *const all_functions,
    register_id const current_function_result, instruction const input,
    size_t const indentation, stream_writer const c_output)
{
    switch (input.type)
    {
    case instruction_call:
        LPG_TRY(indent(indentation, c_output));
        switch (state->registers[input.call.callee].meaning)
        {
        case register_meaning_nothing:
        case register_meaning_global:
            LPG_UNREACHABLE();

        case register_meaning_variable:
            LPG_TO_DO();

        case register_meaning_print:
            state->standard_library.using_stdio = true;
            state->standard_library.using_unit = true;
            set_register_variable(
                state, input.call.result, register_resource_ownership_none);
            LPG_TRY(stream_writer_write_string(c_output, "unit const "));
            LPG_TRY(generate_register_name(input.call.result, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = unit_impl;\n"));
            LPG_TRY(indent(indentation, c_output));
            LPG_TRY(stream_writer_write_string(c_output, "fwrite("));
            ASSERT(input.call.argument_count == 1);
            LPG_TRY(generate_c_str(state, input.call.arguments[0], c_output));
            LPG_TRY(stream_writer_write_string(c_output, ", 1, "));
            LPG_TRY(generate_string_length(
                state, input.call.arguments[0], c_output));
            LPG_TRY(stream_writer_write_string(c_output, ", stdout);\n"));
            return success;

        case register_meaning_read:
            state->standard_library.using_stdio = true;
            standard_library_usage_use_string_ref(&state->standard_library);
            state->standard_library.using_read = true;
            ASSERT(input.call.argument_count == 0);
            set_register_variable(state, input.call.result,
                                  register_resource_ownership_owns_string);
            LPG_TRY(stream_writer_write_string(c_output, "string_ref const "));
            LPG_TRY(generate_register_name(input.call.result, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = read_impl();\n"));
            return success;

        case register_meaning_assert:
            state->standard_library.using_assert = true;
            set_register_variable(
                state, input.call.result, register_resource_ownership_none);
            LPG_TRY(stream_writer_write_string(c_output, "unit const "));
            LPG_TRY(generate_register_name(input.call.result, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = assert_impl("));
            ASSERT(input.call.argument_count == 1);
            LPG_TRY(generate_c_read_access(
                state, input.call.arguments[0], c_output));
            LPG_TRY(stream_writer_write_string(c_output, ");\n"));
            return success;

        case register_meaning_string_equals:
            standard_library_usage_use_string_ref(&state->standard_library);
            set_register_variable(
                state, input.call.result, register_resource_ownership_none);
            LPG_TRY(stream_writer_write_string(c_output, "bool const "));
            LPG_TRY(generate_register_name(input.call.result, c_output));
            LPG_TRY(
                stream_writer_write_string(c_output, " = string_ref_equals("));
            ASSERT(input.call.argument_count == 2);
            LPG_TRY(generate_c_read_access(
                state, input.call.arguments[0], c_output));
            LPG_TRY(stream_writer_write_string(c_output, ", "));
            LPG_TRY(generate_c_read_access(
                state, input.call.arguments[1], c_output));
            LPG_TRY(stream_writer_write_string(c_output, ");\n"));
            return success;

        case register_meaning_concat:
            standard_library_usage_use_string_ref(&state->standard_library);
            set_register_variable(state, input.call.result,
                                  register_resource_ownership_owns_string);
            LPG_TRY(stream_writer_write_string(c_output, "string_ref const "));
            LPG_TRY(generate_register_name(input.call.result, c_output));
            LPG_TRY(
                stream_writer_write_string(c_output, " = string_ref_concat("));
            ASSERT(input.call.argument_count == 2);
            LPG_TRY(generate_c_read_access(
                state, input.call.arguments[0], c_output));
            LPG_TRY(stream_writer_write_string(c_output, ", "));
            LPG_TRY(generate_c_read_access(
                state, input.call.arguments[1], c_output));
            LPG_TRY(stream_writer_write_string(c_output, ");\n"));
            return success;

        case register_meaning_literal:
        {
            type const result_type =
                signature_of(state, state->registers[input.call.callee]
                                        .literal.function_pointer)
                    .result;
            set_register_variable(
                state, input.call.result,
                find_register_resource_ownership(result_type));
            LPG_TRY(generate_type(result_type, &state->standard_library,
                                  state->definitions, c_output));
            break;
        }

        case register_meaning_and:
            ASSUME(input.call.argument_count == 2);
            set_register_variable(
                state, input.call.result, register_resource_ownership_none);
            LPG_TRY(stream_writer_write_string(c_output, "size_t const "));
            LPG_TRY(generate_register_name(input.call.result, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = ("));
            LPG_TRY(generate_c_read_access(
                state, input.call.arguments[0], c_output));
            LPG_TRY(stream_writer_write_string(c_output, " & "));
            LPG_TRY(generate_c_read_access(
                state, input.call.arguments[1], c_output));
            LPG_TRY(stream_writer_write_string(c_output, ");\n"));
            return success;

        case register_meaning_or:
            ASSUME(input.call.argument_count == 2);
            set_register_variable(
                state, input.call.result, register_resource_ownership_none);
            LPG_TRY(stream_writer_write_string(c_output, "size_t const "));
            LPG_TRY(generate_register_name(input.call.result, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = ("));
            LPG_TRY(generate_c_read_access(
                state, input.call.arguments[0], c_output));
            LPG_TRY(stream_writer_write_string(c_output, " | "));
            LPG_TRY(generate_c_read_access(
                state, input.call.arguments[1], c_output));
            LPG_TRY(stream_writer_write_string(c_output, ");\n"));
            return success;

        case register_meaning_not:
            ASSUME(input.call.argument_count == 1);
            set_register_variable(
                state, input.call.result, register_resource_ownership_none);
            LPG_TRY(stream_writer_write_string(c_output, "size_t const "));
            LPG_TRY(generate_register_name(input.call.result, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = !"));
            LPG_TRY(generate_c_read_access(
                state, input.call.arguments[0], c_output));
            LPG_TRY(stream_writer_write_string(c_output, ";\n"));
            return success;

        case register_meaning_argument:
            LPG_TO_DO();
        }
        LPG_TRY(stream_writer_write_string(c_output, " const "));
        LPG_TRY(generate_register_name(input.call.result, c_output));
        LPG_TRY(stream_writer_write_string(c_output, " = "));
        LPG_TRY(generate_c_read_access(state, input.call.callee, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "("));
        for (size_t i = 0; i < input.call.argument_count; ++i)
        {
            if (i > 0)
            {
                LPG_TRY(stream_writer_write_string(c_output, ", "));
            }
            LPG_TRY(generate_c_read_access(
                state, input.call.arguments[i], c_output));
        }
        LPG_TRY(stream_writer_write_string(c_output, ");\n"));
        return success;

    case instruction_loop:
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "for (;;)\n"));
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "{\n"));
        LPG_TRY(generate_sequence(state, all_functions, current_function_result,
                                  input.loop, indentation + 1, c_output));
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "}\n"));
        return success;

    case instruction_global:
        set_register_meaning(state, input.global_into, register_meaning_global);
        return success;

    case instruction_read_struct:
        switch (state->registers[input.read_struct.from_object].meaning)
        {
        case register_meaning_nothing:
        case register_meaning_and:
        case register_meaning_not:
        case register_meaning_or:
            LPG_UNREACHABLE();

        case register_meaning_argument:
            LPG_TO_DO();

        case register_meaning_global:
            switch (input.read_struct.member)
            {
            case 2:
                set_register_meaning(
                    state, input.read_struct.into, register_meaning_print);
                return success;

            case 4:
                set_register_meaning(
                    state, input.read_struct.into, register_meaning_assert);
                return success;

            case 5:
                set_register_meaning(
                    state, input.read_struct.into, register_meaning_and);
                return success;

            case 6:
                set_register_meaning(
                    state, input.read_struct.into, register_meaning_or);
                return success;

            case 7:
                set_register_meaning(
                    state, input.read_struct.into, register_meaning_not);
                return success;

            case 8:
                set_register_meaning(
                    state, input.read_struct.into, register_meaning_concat);
                return success;

            case 9:
                set_register_meaning(state, input.read_struct.into,
                                     register_meaning_string_equals);
                return success;

            case 10:
                set_register_meaning(
                    state, input.read_struct.into, register_meaning_read);
                return success;

            case 14:
                set_register_literal(
                    state, input.read_struct.into, value_from_unit());
                return success;

            default:
                LPG_TO_DO();
            }

        case register_meaning_variable:
            LPG_TO_DO();

        case register_meaning_print:
        case register_meaning_read:
        case register_meaning_assert:
        case register_meaning_string_equals:
        case register_meaning_concat:
            LPG_UNREACHABLE();

        case register_meaning_literal:
            LPG_TO_DO();
        }

    case instruction_break:
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "break;\n"));
        return success;

    case instruction_literal:
        ASSERT(state->registers[input.literal.into].meaning ==
               register_meaning_nothing);
        set_register_literal(state, input.literal.into, input.literal.value_);
        return success;

    case instruction_tuple:
    case instruction_enum_construct:
        LPG_TO_DO();
    }
    LPG_UNREACHABLE();
}

static success_indicator
generate_sequence(c_backend_state *state,
                  checked_function const *const all_functions,
                  register_id const current_function_result,
                  instruction_sequence const sequence, size_t const indentation,
                  stream_writer const c_output)
{
    size_t const previously_active_registers = state->active_register_count;
    for (size_t i = 0; i < sequence.length; ++i)
    {
        LPG_TRY(
            generate_instruction(state, all_functions, current_function_result,
                                 sequence.elements[i], indentation, c_output));
    }
    for (size_t i = previously_active_registers;
         i < state->active_register_count; ++i)
    {
        register_id const which = state->active_registers[i];
        if (which == current_function_result)
        {
            continue;
        }
        switch (state->registers[which].ownership)
        {
        case register_resource_ownership_none:
            break;

        case register_resource_ownership_owns_string:
            ASSERT(state->standard_library.using_string_ref);
            LPG_TRY(indent(indentation, c_output));
            LPG_TRY(stream_writer_write_string(c_output, "string_ref_free(&"));
            LPG_TRY(generate_register_name(which, c_output));
            LPG_TRY(stream_writer_write_string(c_output, ");\n"));
            break;

        case register_resource_ownership_borrows_string:
            break;
        }
    }
    state->active_register_count = previously_active_registers;
    return success;
}

static success_indicator
generate_function_body(checked_function const current_function,
                       checked_function const *const all_functions,
                       stream_writer const c_output,
                       standard_library_usage *standard_library,
                       type_definitions *const definitions, bool const return_0)
{
    LPG_TRY(stream_writer_write_string(c_output, "{\n"));

    c_backend_state state = {
        allocate_array(
            current_function.number_of_registers, sizeof(*state.registers)),
        *standard_library, NULL, 0, all_functions, definitions};
    for (size_t j = 0; j < current_function.number_of_registers; ++j)
    {
        state.registers[j].meaning = register_meaning_nothing;
        state.registers[j].ownership = register_resource_ownership_none;
    }
    for (register_id i = 0; i < current_function.signature->arity; ++i)
    {
        set_register_argument(
            &state, i, i, (current_function.signature->arguments[i].kind ==
                           type_kind_string_ref)
                              ? register_resource_ownership_borrows_string
                              : register_resource_ownership_none);
    }
    LPG_TRY(generate_sequence(&state, all_functions,
                              current_function.return_value,
                              current_function.body, 1, c_output));

    if (!return_0)
    {
        LPG_TRY(generate_add_reference_for_return_value(
            &state, current_function.return_value, c_output));
    }

    LPG_TRY(stream_writer_write_string(c_output, "    return "));
    if (return_0)
    {
        LPG_TRY(stream_writer_write_string(c_output, "0"));
    }
    else
    {
        LPG_TRY(generate_c_read_access(
            &state, current_function.return_value, c_output));
    }
    LPG_TRY(stream_writer_write_string(c_output, ";\n"));
    LPG_TRY(stream_writer_write_string(c_output, "}\n"));
    *standard_library = state.standard_library;
    deallocate(state.registers);
    if (state.active_registers)
    {
        deallocate(state.active_registers);
    }
    return success;
}

static success_indicator
generate_function_declaration(function_id const id,
                              function_pointer const signature,
                              standard_library_usage *const standard_library,
                              type_definitions *const definitions,
                              stream_writer const program_defined_writer)
{
    LPG_TRY(stream_writer_write_string(program_defined_writer, "static "));
    LPG_TRY(generate_type(signature.result, standard_library, definitions,
                          program_defined_writer));
    LPG_TRY(stream_writer_write_string(program_defined_writer, " "));
    LPG_TRY(generate_function_name(id, program_defined_writer));
    LPG_TRY(stream_writer_write_string(program_defined_writer, "("));
    if (signature.arity == 0)
    {
        LPG_TRY(stream_writer_write_string(program_defined_writer, "void"));
    }
    for (register_id j = 0; j < signature.arity; ++j)
    {
        if (j > 0)
        {
            LPG_TRY(stream_writer_write_string(program_defined_writer, ", "));
        }
        LPG_TRY(generate_type(signature.arguments[j], standard_library,
                              definitions, program_defined_writer));
        LPG_TRY(stream_writer_write_string(program_defined_writer, " const "));
        LPG_TRY(generate_parameter_name(j, program_defined_writer));
    }
    LPG_TRY(stream_writer_write_string(program_defined_writer, ")"));
    return success;
}

success_indicator generate_c(checked_program const program,
                             stream_writer const c_output)
{
    memory_writer program_defined = {NULL, 0, 0};
    stream_writer program_defined_writer =
        memory_writer_erase(&program_defined);

    standard_library_usage standard_library = {
        false, false, false, false, false, false};

    type_definitions definitions = {NULL, 0};

    for (function_id i = 1; i < program.function_count; ++i)
    {
        checked_function const current_function = program.functions[i];
        LPG_TRY_GOTO(generate_function_declaration(
                         i, *current_function.signature, &standard_library,
                         &definitions, program_defined_writer),
                     fail);
        LPG_TRY_GOTO(
            stream_writer_write_string(program_defined_writer, ";\n"), fail);
    }

    for (function_id i = 1; i < program.function_count; ++i)
    {
        checked_function const current_function = program.functions[i];
        LPG_TRY_GOTO(generate_function_declaration(
                         i, *current_function.signature, &standard_library,
                         &definitions, program_defined_writer),
                     fail);
        LPG_TRY_GOTO(
            stream_writer_write_string(program_defined_writer, "\n"), fail);
        LPG_TRY_GOTO(
            generate_function_body(current_function, program.functions,
                                   program_defined_writer, &standard_library,
                                   &definitions, false),
            fail);
    }

    LPG_TRY_GOTO(
        stream_writer_write_string(program_defined_writer, "int main(void)\n"),
        fail);
    LPG_TRY_GOTO(generate_function_body(program.functions[0], program.functions,
                                        program_defined_writer,
                                        &standard_library, &definitions, true),
                 fail);

    if (standard_library.using_unit)
    {
        LPG_TRY_GOTO(
            stream_writer_write_string(c_output, "#include <lpg_std_unit.h>\n"),
            fail_2);
    }
    if (standard_library.using_assert)
    {
        LPG_TRY_GOTO(stream_writer_write_string(
                         c_output, "#include <lpg_std_assert.h>\n"),
                     fail_2);
    }
    if (standard_library.using_string_ref)
    {
        LPG_TRY_GOTO(stream_writer_write_string(
                         c_output, "#include <lpg_std_string.h>\n"),
                     fail_2);
    }
    if (standard_library.using_stdio)
    {
        LPG_TRY_GOTO(
            stream_writer_write_string(c_output, "#include <stdio.h>\n"),
            fail_2);
    }
    if (standard_library.using_read)
    {
        LPG_TRY_GOTO(
            stream_writer_write_string(c_output, "#include <lpg_std_read.h>\n"),
            fail_2);
    }
    if (standard_library.using_stdint)
    {
        LPG_TRY_GOTO(
            stream_writer_write_string(c_output, "#include <stdint.h>\n"),
            fail_2);
    }

    for (size_t i = 0; i < definitions.count; ++i)
    {
        LPG_TRY_GOTO(stream_writer_write_unicode_view(
                         c_output, unicode_view_from_string(
                                       definitions.elements[i].definition)),
                     fail_2);
    }

    LPG_TRY_GOTO(stream_writer_write_bytes(
                     c_output, program_defined.data, program_defined.used),
                 fail_2);
    type_definitions_free(definitions);
    memory_writer_free(&program_defined);
    return success;

fail_2:
fail:
    type_definitions_free(definitions);
    memory_writer_free(&program_defined);
    return failure;
}
