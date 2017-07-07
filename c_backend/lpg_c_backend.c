#include "lpg_c_backend.h"
#include "lpg_assert.h"
#include "lpg_instruction.h"
#include "lpg_allocate.h"

typedef struct standard_library_usage
{
    bool using_string_ref;
    bool using_read;
    bool using_stdio;
    bool using_assert;
    bool using_stdlib;
    bool using_stdbool;
    bool using_string;
} standard_library_usage;

static void standard_library_usage_use_string_ref(standard_library_usage *usage)
{
    usage->using_string_ref = true;
    usage->using_stdbool = true;
    usage->using_string = true;
    usage->using_stdlib = true;
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
    register_meaning_function,
    register_meaning_argument
} register_meaning;

typedef enum register_resource_ownership
{
    register_resource_ownership_none,
    register_resource_ownership_string_ref
} register_resource_ownership;

typedef struct register_state
{
    register_meaning meaning;
    register_resource_ownership ownership;
    union
    {
        value literal;
        function_id function;
        register_id argument;
    };
} register_state;

typedef struct c_backend_state
{
    register_state *registers;
    standard_library_usage standard_library;
    register_id *active_registers;
    size_t active_register_count;
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

static void set_register_lambda(c_backend_state *const state,
                                register_id const id,
                                function_id const function)
{
    ASSERT(state->registers[id].meaning == register_meaning_nothing);
    state->registers[id].meaning = register_meaning_function;
    state->registers[id].function = function;
    active_register(state, id);
}

static void set_register_argument(c_backend_state *const state,
                                  register_id const id,
                                  register_id const argument)
{
    ASSERT(state->registers[id].meaning == register_meaning_nothing);
    state->registers[id].meaning = register_meaning_argument;
    state->registers[id].argument = argument;
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

static success_indicator generate_type(type const generated,
                                       stream_writer const c_output)
{
    switch (generated.kind)
    {
    case type_kind_structure:
    case type_kind_function_pointer:
        LPG_TO_DO();

    case type_kind_unit:
        return stream_writer_write_string(c_output, "unit");

    case type_kind_string_ref:
        LPG_TO_DO();

    case type_kind_enumeration:
        return stream_writer_write_string(c_output, "size_t");

    case type_kind_type:
    case type_kind_integer_range:
    case type_kind_inferred:
        LPG_TO_DO();
    }
    UNREACHABLE();
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
        LPG_TO_DO();

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

    case register_meaning_function:
        return generate_function_name(
            state->registers[from].function, c_output);

    case register_meaning_literal:
        switch (state->registers[from].literal.kind)
        {
        case value_kind_integer:
            LPG_TO_DO();

        case value_kind_string:
            LPG_TRY(stream_writer_write_string(c_output, "string_ref_create("));
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
        case value_kind_flat_object:
        case value_kind_type:
            LPG_TO_DO();

        case value_kind_enum_element:
        {
            char buffer[64];
            char const *const formatted = integer_format(
                integer_create(0, state->registers[from].literal.enum_element),
                lower_case_digits, 10, buffer, sizeof(buffer));
            return stream_writer_write_bytes(
                c_output, formatted,
                (size_t)((buffer + sizeof(buffer)) - formatted));
        }

        case value_kind_unit:
            return stream_writer_write_string(c_output, "unit_value");
        }

    case register_meaning_argument:
        return generate_parameter_name(
            state->registers[from].argument, c_output);
    }
    UNREACHABLE();
}

static success_indicator generate_c_str(c_backend_state *state,
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
        LPG_TRY(generate_register_name(from, c_output));
        return stream_writer_write_string(c_output, ".data");

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

    case register_meaning_function:
        LPG_TO_DO();

    case register_meaning_literal:
        switch (state->registers[from].literal.kind)
        {
        case value_kind_integer:
            LPG_TO_DO();

        case value_kind_string:
            return encode_string_literal(
                state->registers[from].literal.string_ref, c_output);

        case value_kind_function_pointer:
        case value_kind_flat_object:
        case value_kind_type:
        case value_kind_enum_element:
        case value_kind_unit:
            LPG_TO_DO();
        }
    }
    UNREACHABLE();
}

static success_indicator generate_string_length(c_backend_state *state,
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
        LPG_TRY(generate_register_name(from, c_output));
        return stream_writer_write_string(c_output, ".length");

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

    case register_meaning_function:
        LPG_TO_DO();

    case register_meaning_literal:
        switch (state->registers[from].literal.kind)
        {
        case value_kind_integer:
            LPG_TO_DO();

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
            LPG_TO_DO();
        }
        LPG_TO_DO();
    }
    UNREACHABLE();
}

static success_indicator generate_sequence(c_backend_state *state,
                                           instruction_sequence const sequence,
                                           size_t const indentation,
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

static success_indicator generate_instruction(c_backend_state *state,
                                              instruction const input,
                                              size_t const indentation,
                                              stream_writer const c_output)
{
    switch (input.type)
    {
    case instruction_call:
        LPG_TRY(indent(indentation, c_output));
        switch (state->registers[input.call.callee].meaning)
        {
        case register_meaning_nothing:
            LPG_TO_DO();

        case register_meaning_global:
            LPG_TO_DO();

        case register_meaning_variable:
            LPG_TO_DO();

        case register_meaning_print:
            state->standard_library.using_stdio = true;
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
                                  register_resource_ownership_string_ref);
            LPG_TRY(stream_writer_write_string(c_output, "string_ref const "));
            LPG_TRY(generate_register_name(input.call.result, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = read_impl();\n"));
            return success;

        case register_meaning_assert:
            state->standard_library.using_assert = true;
            state->standard_library.using_stdlib = true;
            state->standard_library.using_stdbool = true;
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
                                  register_resource_ownership_string_ref);
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
            LPG_TO_DO();

        case register_meaning_function:
            break;

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
        }
        set_register_variable(
            state, input.call.result, register_resource_ownership_none);
        LPG_TRY(stream_writer_write_string(c_output, "unit const "));
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
        LPG_TRY(
            generate_sequence(state, input.loop, indentation + 1, c_output));
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

            default:
                LPG_TO_DO();
            }

        case register_meaning_variable:
            LPG_TO_DO();

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
            LPG_TO_DO();

        case register_meaning_function:
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

    case instruction_lambda:
        set_register_lambda(state, input.lambda.into, input.lambda.id);
        return success;
    }
    UNREACHABLE();
}

static success_indicator generate_sequence(c_backend_state *state,
                                           instruction_sequence const sequence,
                                           size_t const indentation,
                                           stream_writer const c_output)
{
    size_t const previously_active_registers = state->active_register_count;
    for (size_t i = 0; i < sequence.length; ++i)
    {
        LPG_TRY(generate_instruction(
            state, sequence.elements[i], indentation, c_output));
    }
    for (size_t i = previously_active_registers;
         i < state->active_register_count; ++i)
    {
        register_id const which = state->active_registers[i];
        switch (state->registers[which].ownership)
        {
        case register_resource_ownership_none:
            break;

        case register_resource_ownership_string_ref:
            ASSERT(state->standard_library.using_string_ref);
            LPG_TRY(indent(indentation, c_output));
            LPG_TRY(stream_writer_write_string(c_output, "string_ref_free(&"));
            LPG_TRY(generate_register_name(which, c_output));
            LPG_TRY(stream_writer_write_string(c_output, ");\n"));
            break;
        }
    }
    state->active_register_count = previously_active_registers;
    return success;
}

static success_indicator generate_function_body(
    checked_function const current_function, stream_writer const c_output,
    standard_library_usage *standard_library, bool const return_0)
{
    LPG_TRY(stream_writer_write_string(c_output, "{\n"));

    c_backend_state state = {
        allocate_array(
            current_function.number_of_registers, sizeof(*state.registers)),
        *standard_library, NULL, 0};
    for (size_t j = 0; j < current_function.number_of_registers; ++j)
    {
        state.registers[j].meaning = register_meaning_nothing;
        state.registers[j].ownership = register_resource_ownership_none;
    }
    for (register_id i = 0; i < current_function.signature->arity; ++i)
    {
        set_register_argument(&state, i, i);
    }
    LPG_TRY(generate_sequence(&state, current_function.body, 1, c_output));
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

success_indicator generate_c(checked_program const program,
                             stream_writer const c_output)
{
    memory_writer program_defined = {NULL, 0, 0};
    stream_writer program_defined_writer =
        memory_writer_erase(&program_defined);

    standard_library_usage standard_library = {
        false, false, false, false, false, false, false};
    for (function_id i = 1; i < program.function_count; ++i)
    {
        checked_function const current_function = program.functions[i];
        LPG_TRY_GOTO(
            stream_writer_write_string(program_defined_writer, "static "),
            fail);
        LPG_TRY_GOTO(generate_type(current_function.signature->result,
                                   program_defined_writer),
                     fail);
        LPG_TRY_GOTO(
            stream_writer_write_string(program_defined_writer, " "), fail);
        LPG_TRY_GOTO(generate_function_name(i, program_defined_writer), fail);
        LPG_TRY_GOTO(
            stream_writer_write_string(program_defined_writer, "("), fail);
        if (current_function.signature->arity == 0)
        {
            LPG_TRY_GOTO(
                stream_writer_write_string(program_defined_writer, "void"),
                fail);
        }
        for (register_id j = 0; j < current_function.signature->arity; ++j)
        {
            if (j > 0)
            {
                LPG_TRY_GOTO(
                    stream_writer_write_string(program_defined_writer, ", "),
                    fail);
            }
            LPG_TRY_GOTO(generate_type(current_function.signature->arguments[j],
                                       program_defined_writer),
                         fail);
            LPG_TRY_GOTO(
                stream_writer_write_string(program_defined_writer, " "), fail);
            LPG_TRY_GOTO(
                generate_parameter_name(j, program_defined_writer), fail);
        }
        LPG_TRY_GOTO(
            stream_writer_write_string(program_defined_writer, ")\n"), fail);
        LPG_TRY_GOTO(
            generate_function_body(current_function, program_defined_writer,
                                   &standard_library, false),
            fail);
    }

    LPG_TRY_GOTO(
        stream_writer_write_string(program_defined_writer, "int main(void)\n"),
        fail);
    LPG_TRY_GOTO(
        generate_function_body(program.functions[0], program_defined_writer,
                               &standard_library, true),
        fail);

    if (standard_library.using_stdlib)
    {
        LPG_TRY_GOTO(
            stream_writer_write_string(c_output, LPG_C_STDLIB), fail_2);
    }
    if (standard_library.using_stdbool)
    {
        LPG_TRY_GOTO(
            stream_writer_write_string(c_output, LPG_C_STDBOOL), fail_2);
    }
    if (standard_library.using_assert)
    {
        LPG_TRY_GOTO(
            stream_writer_write_string(c_output, LPG_C_ASSERT), fail_2);
    }
    if (standard_library.using_string)
    {
        LPG_TRY_GOTO(
            stream_writer_write_string(c_output, LPG_C_STRING), fail_2);
    }
    if (standard_library.using_string_ref)
    {
        LPG_TRY_GOTO(
            stream_writer_write_string(c_output, LPG_C_STRING_REF), fail_2);
    }
    if (standard_library.using_stdio)
    {
        LPG_TRY_GOTO(stream_writer_write_string(c_output, LPG_C_STDIO), fail_2);
    }
    if (standard_library.using_read)
    {
        LPG_TRY_GOTO(stream_writer_write_string(c_output, LPG_C_READ), fail_2);
    }

    LPG_TRY_GOTO(stream_writer_write_bytes(
                     c_output, program_defined.data, program_defined.used),
                 fail_2);
    memory_writer_free(&program_defined);
    return success;

fail_2:
fail:
    memory_writer_free(&program_defined);
    return failure;
}
