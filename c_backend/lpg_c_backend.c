#include "lpg_c_backend.h"
#include "lpg_assert.h"
#include "lpg_instruction.h"
#include "lpg_allocate.h"

typedef struct standard_library_usage
{
    bool using_string_ref;
    bool using_read;
    bool using_stdio;
} standard_library_usage;

typedef enum register_meaning
{
    register_meaning_nothing,
    register_meaning_global,
    register_meaning_variable,
    register_meaning_string_literal,
    register_meaning_print,
    register_meaning_read,
    register_meaning_unit
} register_meaning;

typedef enum register_resource_ownership
{
    register_resource_ownership_none,
    register_resource_ownership_string_ref
} register_resource_ownership;

typedef struct register_state
{
    register_meaning meaning;
    unicode_view string_literal;
    register_resource_ownership ownership;
} register_state;

typedef struct c_backend_state
{
    register_state *registers;
    standard_library_usage standard_library;
} c_backend_state;

static void set_register_meaning(c_backend_state *const state,
                                 size_t const register_id,
                                 register_meaning const meaning)
{
    ASSERT(state->registers[register_id].meaning == register_meaning_nothing);
    ASSERT(meaning != register_meaning_nothing);
    ASSERT(meaning != register_meaning_string_literal);
    ASSERT(meaning != register_meaning_variable);
    state->registers[register_id].meaning = meaning;
}

static void set_register_variable(c_backend_state *const state,
                                  size_t const register_id,
                                  register_resource_ownership ownership)
{
    ASSERT(state->registers[register_id].meaning == register_meaning_nothing);
    state->registers[register_id].meaning = register_meaning_variable;
    state->registers[register_id].ownership = ownership;
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
        LPG_TO_DO();

    case register_meaning_print:
        LPG_TO_DO();

    case register_meaning_read:
        LPG_TO_DO();

    case register_meaning_unit:
        LPG_TO_DO();

    case register_meaning_string_literal:
        return encode_string_literal(
            state->registers[from].string_literal, c_output);
    }
    UNREACHABLE();
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

    case register_meaning_unit:
        LPG_TO_DO();

    case register_meaning_string_literal:
        return encode_string_literal(
            state->registers[from].string_literal, c_output);
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

    case register_meaning_unit:
        LPG_TO_DO();

    case register_meaning_string_literal:
    {
        char buffer[40];
        char *formatted = integer_format(
            integer_create(0, state->registers[from].string_literal.length),
            lower_case_digits, 10, buffer, sizeof(buffer));
        return stream_writer_write_bytes(
            c_output, formatted,
            (size_t)((buffer + sizeof(buffer)) - formatted));
    }
    }
    UNREACHABLE();
}

static register_state make_string_literal(unicode_view const value)
{
    register_state result = {register_meaning_string_literal, value};
    return result;
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
            state->standard_library.using_string_ref = true;
            state->standard_library.using_read = true;
            ASSERT(input.call.argument_count == 0);
            set_register_variable(state, input.call.result,
                                  register_resource_ownership_string_ref);
            LPG_TRY(stream_writer_write_string(c_output, "string_ref const "));
            LPG_TRY(generate_register_name(input.call.result, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = read_impl();\n"));
            return success;

        case register_meaning_unit:
            LPG_TO_DO();

        case register_meaning_string_literal:
            LPG_TO_DO();
        }
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

        case register_meaning_unit:
            LPG_TO_DO();

        case register_meaning_string_literal:
            LPG_TO_DO();
        }

    case instruction_break:
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "break;\n"));
        return success;

    case instruction_literal:
        ASSERT(state->registers[input.literal.into].meaning ==
               register_meaning_nothing);
        switch (input.literal.value_.kind)
        {
        case value_kind_integer:
            LPG_TO_DO();

        case value_kind_string:
            state->registers[input.literal.into] =
                make_string_literal(input.literal.value_.string_ref);
            return success;

        case value_kind_function_pointer:
            LPG_TO_DO();

        case value_kind_flat_object:
            LPG_TO_DO();

        case value_kind_type:
            LPG_TO_DO();

        case value_kind_enum_element:
            LPG_TO_DO();

        case value_kind_unit:
            state->registers[input.literal.into].meaning =
                register_meaning_unit;
            return success;
        }
        UNREACHABLE();
    }
    UNREACHABLE();
}

static success_indicator generate_sequence(c_backend_state *state,
                                           instruction_sequence const sequence,
                                           size_t const indentation,
                                           stream_writer const c_output)
{
    for (size_t i = 0; i < sequence.length; ++i)
    {
        LPG_TRY(generate_instruction(
            state, sequence.elements[i], indentation, c_output));
    }
    return success;
}

success_indicator generate_c(checked_program const program,
                             stream_writer const c_output)
{
    /*TODO: support multiple functions*/
    ASSERT(program.function_count == 1);

    memory_writer program_defined = {NULL, 0, 0};
    stream_writer program_defined_writer =
        memory_writer_erase(&program_defined);

    LPG_TRY_GOTO(
        stream_writer_write_string(program_defined_writer, "int main(void)\n"),
        fail);
    LPG_TRY_GOTO(
        stream_writer_write_string(program_defined_writer, "{\n"), fail);

    c_backend_state state = {
        allocate_array(
            program.functions[0].number_of_registers, sizeof(*state.registers)),
        {false, false, false}};
    for (size_t i = 0; i < program.functions[0].number_of_registers; ++i)
    {
        state.registers[i].meaning = register_meaning_nothing;
        state.registers[i].ownership = register_resource_ownership_none;
    }
    LPG_TRY_GOTO(generate_sequence(&state, program.functions[0].body, 1,
                                   program_defined_writer),
                 fail_2);

    for (register_id i = 0; i < program.functions[0].number_of_registers; ++i)
    {
        switch (state.registers[i].ownership)
        {
        case register_resource_ownership_none:
            break;

        case register_resource_ownership_string_ref:
            ASSERT(state.standard_library.using_string_ref);
            LPG_TRY_GOTO(stream_writer_write_string(
                             program_defined_writer, "    string_ref_free(&"),
                         fail_2);
            LPG_TRY_GOTO(
                generate_register_name(i, program_defined_writer), fail_2);
            LPG_TRY_GOTO(
                stream_writer_write_string(program_defined_writer, ");\n"),
                fail_2);
            break;
        }
    }

    LPG_TRY_GOTO(
        stream_writer_write_string(program_defined_writer, "    return 0;\n"),
        fail_2);
    LPG_TRY_GOTO(
        stream_writer_write_string(program_defined_writer, "}\n"), fail_2);

    if (state.standard_library.using_string_ref)
    {
        LPG_TRY_GOTO(
            stream_writer_write_string(c_output, LPG_C_STRING_REF), fail_2);
    }
    if (state.standard_library.using_stdio)
    {
        LPG_TRY_GOTO(stream_writer_write_string(c_output, LPG_C_STDIO), fail_2);
    }
    if (state.standard_library.using_read)
    {
        LPG_TRY_GOTO(stream_writer_write_string(c_output, LPG_C_READ), fail_2);
    }

    LPG_TRY_GOTO(stream_writer_write_bytes(
                     c_output, program_defined.data, program_defined.used),
                 fail_2);
    memory_writer_free(&program_defined);
    deallocate(state.registers);
    return success;

fail_2:
    deallocate(state.registers);

fail:
    memory_writer_free(&program_defined);
    return failure;
}
