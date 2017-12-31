#include "lpg_javascript_backend.h"
#include "lpg_assert.h"
#include "lpg_instruction.h"
#include "lpg_allocate.h"
#include "lpg_structure_member.h"

static success_indicator generate_function_name(function_id const id, stream_writer const javascript_output)
{
    LPG_TRY(stream_writer_write_string(javascript_output, "lambda_"));
    LPG_TRY(stream_writer_write_integer(javascript_output, integer_create(0, id)));
    return success;
}

static success_indicator declare_function(function_id const id, stream_writer const javascript_output)
{
    LPG_TRY(stream_writer_write_string(javascript_output, "var "));
    LPG_TRY(generate_function_name(id, javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, ";\n"));
    return success;
}

static success_indicator generate_register_name(register_id const id, stream_writer const javascript_output)
{
    LPG_TRY(stream_writer_write_string(javascript_output, "r_"));
    LPG_TRY(stream_writer_write_integer(javascript_output, integer_create(0, id)));
    return success;
}

static success_indicator encode_string_literal(unicode_view const content, stream_writer const javascript_output)
{
    LPG_TRY(stream_writer_write_string(javascript_output, "\""));
    for (size_t i = 0; i < content.length; ++i)
    {
        switch (content.begin[i])
        {
        case '\n':
            LPG_TRY(stream_writer_write_string(javascript_output, "\\n"));
            break;

        default:
            LPG_TRY(stream_writer_write_bytes(javascript_output, content.begin + i, 1));
        }
    }
    LPG_TRY(stream_writer_write_string(javascript_output, "\""));
    return success;
}

static success_indicator generate_value(value const generated, type const type_of,
                                        checked_function const *all_functions, interface const *const all_interfaces,
                                        stream_writer const javascript_output);

static success_indicator generate_enum_element(enum_element_value const element, checked_function const *all_functions,
                                               interface const *const all_interfaces,
                                               stream_writer const javascript_output)
{
    if (element.state)
    {
        LPG_TRY(stream_writer_write_string(javascript_output, "["));
        LPG_TRY(stream_writer_write_integer(javascript_output, integer_create(0, element.which)));
        LPG_TRY(stream_writer_write_string(javascript_output, ", "));
        LPG_TRY(generate_value(*element.state, element.state_type, all_functions, all_interfaces, javascript_output));
        LPG_TRY(stream_writer_write_string(javascript_output, "]"));
        return success;
    }
    return stream_writer_write_integer(javascript_output, integer_create(0, element.which));
}

static success_indicator generate_enum_constructor(enum_constructor_type const constructor,
                                                   stream_writer const javascript_output)
{
    LPG_TRY(stream_writer_write_string(javascript_output, "function (state) { return ["));
    LPG_TRY(stream_writer_write_integer(javascript_output, integer_create(0, constructor.which)));
    LPG_TRY(stream_writer_write_string(javascript_output, ", state]; }"));
    return success;
}

static success_indicator generate_implementation_name(interface_id const interface_, size_t const implementation_index,
                                                      stream_writer const javascript_output)
{
    LPG_TRY(stream_writer_write_string(javascript_output, "impl_"));
    LPG_TRY(stream_writer_write_integer(javascript_output, integer_create(0, interface_)));
    LPG_TRY(stream_writer_write_string(javascript_output, "_"));
    LPG_TRY(stream_writer_write_integer(javascript_output, integer_create(0, implementation_index)));
    return success;
}

static success_indicator generate_capture_alias(size_t const index, stream_writer const javascript_output)
{
    LPG_TRY(stream_writer_write_string(javascript_output, "capture_"));
    return stream_writer_write_integer(javascript_output, integer_create(0, index));
}

static success_indicator generate_lambda_value_from_registers(checked_function const *all_functions,
                                                              function_id const lambda,
                                                              register_id const *const captures,
                                                              size_t const capture_count,
                                                              stream_writer const javascript_output)
{
    LPG_TRY(stream_writer_write_string(javascript_output, "(function () { "));
    for (register_id i = 0; i < capture_count; ++i)
    {
        LPG_TRY(stream_writer_write_string(javascript_output, "var "));
        LPG_TRY(generate_capture_alias(i, javascript_output));
        LPG_TRY(stream_writer_write_string(javascript_output, " = "));
        LPG_TRY(generate_register_name(captures[i], javascript_output));
        LPG_TRY(stream_writer_write_string(javascript_output, ";\n"));
    }
    LPG_TRY(stream_writer_write_string(javascript_output, "return function ("));
    checked_function const *const function = &all_functions[lambda];
    for (register_id i = 0; i < function->signature->parameters.length; ++i)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(javascript_output, ", "));
        }
        LPG_TRY(generate_register_name(i, javascript_output));
    }
    LPG_TRY(stream_writer_write_string(javascript_output, ") { return "));
    LPG_TRY(generate_function_name(lambda, javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, "("));
    bool comma = false;
    for (register_id i = 0; i < capture_count; ++i)
    {
        if (comma)
        {
            LPG_TRY(stream_writer_write_string(javascript_output, ", "));
        }
        else
        {
            comma = true;
        }
        LPG_TRY(generate_capture_alias(i, javascript_output));
    }
    ASSUME(comma);
    for (register_id i = 0; i < function->signature->parameters.length; ++i)
    {
        LPG_TRY(stream_writer_write_string(javascript_output, ", "));
        LPG_TRY(generate_register_name(i, javascript_output));
    }
    LPG_TRY(stream_writer_write_string(javascript_output, "); }; })()"));
    return success;
}

static success_indicator generate_lambda_value_from_values(checked_function const *all_functions,
                                                           interface const *const all_interfaces,
                                                           function_id const lambda, value const *const captures,
                                                           size_t const capture_count,
                                                           stream_writer const javascript_output)
{
    LPG_TRY(stream_writer_write_string(javascript_output, "function ("));
    checked_function const *const function = &all_functions[lambda];
    for (register_id i = 0; i < function->signature->parameters.length; ++i)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(javascript_output, ", "));
        }
        LPG_TRY(generate_register_name(i, javascript_output));
    }
    LPG_TRY(stream_writer_write_string(javascript_output, ") { return "));
    LPG_TRY(generate_function_name(lambda, javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, "("));
    bool comma = false;
    for (register_id i = 0; i < capture_count; ++i)
    {
        if (comma)
        {
            LPG_TRY(stream_writer_write_string(javascript_output, ", "));
        }
        else
        {
            comma = true;
        }
        LPG_TRY(generate_value(
            captures[i], function->signature->captures.elements[i], all_functions, all_interfaces, javascript_output));
    }
    ASSUME(comma);
    for (register_id i = 0; i < function->signature->parameters.length; ++i)
    {
        LPG_TRY(stream_writer_write_string(javascript_output, ", "));
        LPG_TRY(generate_register_name(i, javascript_output));
    }
    LPG_TRY(stream_writer_write_string(javascript_output, "); }"));
    return success;
}

static success_indicator generate_value(value const generated, type const type_of,
                                        checked_function const *all_functions, interface const *const all_interfaces,
                                        stream_writer const javascript_output)
{
    switch (generated.kind)
    {
    case value_kind_type_erased:
        LPG_TRY(stream_writer_write_string(javascript_output, "new "));
        LPG_TRY(generate_implementation_name(
            generated.type_erased.impl.target, generated.type_erased.impl.implementation_index, javascript_output));
        LPG_TRY(stream_writer_write_string(javascript_output, "("));
        LPG_TRY(generate_value(*generated.type_erased.self,
                               all_interfaces[generated.type_erased.impl.target]
                                   .implementations[generated.type_erased.impl.implementation_index]
                                   .self,
                               all_functions, all_interfaces, javascript_output));
        LPG_TRY(stream_writer_write_string(javascript_output, ")"));
        return success;

    case value_kind_integer:
        if (integer_less(integer_create(0, UINT32_MAX), generated.integer_))
        {
            LPG_TO_DO();
        }
        return stream_writer_write_integer(javascript_output, generated.integer_);

    case value_kind_string:
        return encode_string_literal(generated.string_ref, javascript_output);

    case value_kind_function_pointer:
        ASSUME(!generated.function_pointer.external);
        if (generated.function_pointer.capture_count > 0)
        {
            return generate_lambda_value_from_values(all_functions, all_interfaces, generated.function_pointer.code,
                                                     generated.function_pointer.captures,
                                                     generated.function_pointer.capture_count, javascript_output);
        }
        return generate_function_name(generated.function_pointer.code, javascript_output);

    case value_kind_flat_object:
    case value_kind_pattern:
        LPG_TO_DO();

    case value_kind_type:
        return stream_writer_write_string(javascript_output, "/*TODO type*/ undefined");

    case value_kind_enum_element:
        return generate_enum_element(generated.enum_element, all_functions, all_interfaces, javascript_output);

    case value_kind_unit:
        return stream_writer_write_string(javascript_output, "undefined");

    case value_kind_tuple:
    {
        LPG_TRY(stream_writer_write_string(javascript_output, "["));
        ASSUME(type_of.kind == type_kind_tuple);
        ASSUME(generated.tuple_.element_count == type_of.tuple_.length);
        for (size_t i = 0; i < generated.tuple_.element_count; ++i)
        {
            if (i > 0)
            {
                LPG_TRY(stream_writer_write_string(javascript_output, ", "));
            }
            LPG_TRY(generate_value(generated.tuple_.elements[i], type_of.tuple_.elements[i], all_functions,
                                   all_interfaces, javascript_output));
        }
        LPG_TRY(stream_writer_write_string(javascript_output, "]"));
        return success;
    }

    case value_kind_enum_constructor:
        return generate_enum_constructor(*type_of.enum_constructor, javascript_output);
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_var(register_id const id, stream_writer const javascript_output)
{
    LPG_TRY(stream_writer_write_string(javascript_output, "var "));
    LPG_TRY(generate_register_name(id, javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, " = "));
    return success;
}

typedef enum register_type
{
    register_type_none,
    register_type_variable,
    register_type_global,
    register_type_captures
} register_type;

typedef struct register_info
{
    register_type kind;
    type type_of;
} register_info;

static register_info register_info_create(register_type kind, type type_of)
{
    register_info const result = {kind, type_of};
    return result;
}

typedef struct function_generation
{
    register_info *registers;
    checked_function const *all_functions;
    interface const *all_interfaces;
    enumeration const *all_enums;
    structure const *all_structs;
    checked_function const *current_function;
} function_generation;

static function_generation function_generation_create(register_info *registers, checked_function const *all_functions,
                                                      interface const *all_interfaces, enumeration const *all_enums,
                                                      structure const *all_structs,
                                                      checked_function const *current_function)
{
    function_generation const result = {
        registers, all_functions, all_interfaces, all_enums, all_structs, current_function};
    return result;
}

static success_indicator write_register(function_generation *const state, register_id const which, type const type_of,
                                        stream_writer const javascript_output)
{
    ASSUME(state->registers[which].kind == register_type_none);
    state->registers[which] = register_info_create(register_type_variable, type_of);
    LPG_TRY(generate_var(which, javascript_output));
    return success;
}

static success_indicator generate_literal(function_generation *const state, literal_instruction const generated,
                                          checked_function const *all_functions, interface const *const all_interfaces,
                                          stream_writer const javascript_output)
{
    LPG_TRY(write_register(state, generated.into, generated.type_of, javascript_output));
    LPG_TRY(generate_value(generated.value_, generated.type_of, all_functions, all_interfaces, javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, ";\n"));
    return success;
}

static success_indicator generate_capture_name(register_id const index, stream_writer const javascript_output)
{
    LPG_TRY(stream_writer_write_string(javascript_output, "c_"));
    LPG_TRY(stream_writer_write_integer(javascript_output, integer_create(0, index)));
    return success;
}

static success_indicator generate_read_struct_value(function_generation *const state,
                                                    read_struct_instruction const generated,
                                                    stream_writer const javascript_output)
{
    switch (state->registers[generated.from_object].kind)
    {
    case register_type_none:
        LPG_UNREACHABLE();

    case register_type_variable:
        LPG_TRY(generate_register_name(generated.from_object, javascript_output));
        LPG_TRY(stream_writer_write_string(javascript_output, "["));
        LPG_TRY(stream_writer_write_integer(javascript_output, integer_create(0, generated.member)));
        LPG_TRY(stream_writer_write_string(javascript_output, "]"));
        return success;

    case register_type_captures:
        LPG_TRY(generate_capture_name(generated.member, javascript_output));
        return success;

    case register_type_global:
        switch (generated.member)
        {
        case 0:
            return stream_writer_write_string(javascript_output, "undefined");

        case 4:
            return stream_writer_write_string(javascript_output, "assert");

        case 5:
            return stream_writer_write_string(javascript_output, "and");

        case 6:
            return stream_writer_write_string(javascript_output, "or");

        case 7:
            return stream_writer_write_string(javascript_output, "not");

        case 8:
            return stream_writer_write_string(javascript_output, "concat");

        case 9:
            return stream_writer_write_string(javascript_output, "string_equals");

        case 12:
            return stream_writer_write_string(javascript_output, "integer_equals");

        case 13:
            return stream_writer_write_string(javascript_output, "undefined");

        case 14:
            return stream_writer_write_string(javascript_output, "undefined");

        case 16:
            return stream_writer_write_string(javascript_output, "integer_less");

        case 17:
            return stream_writer_write_string(javascript_output, "integer_to_string");

        case 18:
            return stream_writer_write_string(javascript_output, "side_effect");

        default:
            LPG_TO_DO();
        }
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_read_struct(function_generation *const state, read_struct_instruction const generated,
                                              stream_writer const javascript_output)
{
    switch (state->registers[generated.from_object].type_of.kind)
    {
    case type_kind_structure:
    {
        struct_id const structure = state->registers[generated.from_object].type_of.structure_;
        LPG_TRY(write_register(
            state, generated.into, state->all_structs[structure].members[generated.member].what, javascript_output));
        break;
    }

    case type_kind_tuple:
    {
        tuple_type const tuple_ = state->registers[generated.from_object].type_of.tuple_;
        LPG_TRY(write_register(state, generated.into, tuple_.elements[generated.member], javascript_output));
        break;
    }

    case type_kind_function_pointer:
    case type_kind_unit:
    case type_kind_string_ref:
    case type_kind_enumeration:
    case type_kind_type:
    case type_kind_integer_range:
    case type_kind_enum_constructor:
    case type_kind_lambda:
    case type_kind_interface:
    case type_kind_method_pointer:
        LPG_UNREACHABLE();
    }
    LPG_TRY(generate_read_struct_value(state, generated, javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, ";\n"));
    return success;
}

static success_indicator generate_call(function_generation *const state, call_instruction const generated,
                                       stream_writer const javascript_output)
{
    LPG_TRY(write_register(state, generated.result, get_return_type(state->registers[generated.callee].type_of,
                                                                    state->all_functions, state->all_interfaces),
                           javascript_output));
    LPG_TRY(generate_register_name(generated.callee, javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, "("));
    for (size_t i = 0; i < generated.argument_count; ++i)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(javascript_output, ", "));
        }
        LPG_TRY(generate_register_name(generated.arguments[i], javascript_output));
    }
    LPG_TRY(stream_writer_write_string(javascript_output, ");\n"));
    return success;
}

static success_indicator generate_sequence(function_generation *const state, instruction_sequence const body,
                                           stream_writer const javascript_output);

static success_indicator generate_loop(function_generation *const state, instruction_sequence const generated,
                                       stream_writer const javascript_output)
{
    LPG_TRY(stream_writer_write_string(javascript_output, "for (;;)\n"));
    LPG_TRY(stream_writer_write_string(javascript_output, "{\n"));
    LPG_TRY(generate_sequence(state, generated, javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, "}\n"));
    return success;
}

static success_indicator generate_enum_construct(function_generation *const state,
                                                 enum_construct_instruction const construct,
                                                 stream_writer const javascript_output)
{
    LPG_TRY(
        write_register(state, construct.into, type_from_enumeration(construct.which.enumeration), javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, "["));
    LPG_TRY(stream_writer_write_integer(javascript_output, integer_create(0, construct.which.which)));
    LPG_TRY(stream_writer_write_string(javascript_output, ", "));
    LPG_TRY(generate_register_name(construct.state, javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, "];\n"));
    return success;
}

static success_indicator generate_tuple(function_generation *const state, tuple_instruction const generated,
                                        stream_writer const javascript_output)
{
    LPG_TRY(write_register(state, generated.result, type_from_tuple_type(generated.result_type), javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, "["));
    for (size_t i = 0; i < generated.element_count; ++i)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(javascript_output, ", "));
        }
        LPG_TRY(generate_register_name(generated.elements[i], javascript_output));
    }
    LPG_TRY(stream_writer_write_string(javascript_output, "];\n"));
    return success;
}

static success_indicator generate_instantiate_struct(function_generation *const state,
                                                     instantiate_struct_instruction const generated,
                                                     stream_writer const javascript_output)
{
    LPG_TRY(write_register(state, generated.into, type_from_struct(generated.instantiated), javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, "["));
    for (size_t i = 0; i < generated.argument_count; ++i)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(javascript_output, ", "));
        }
        LPG_TRY(generate_register_name(generated.arguments[i], javascript_output));
    }
    LPG_TRY(stream_writer_write_string(javascript_output, "];\n"));
    return success;
}

static success_indicator generate_equality_comparable_match_cases(function_generation *const state,
                                                                  match_instruction const generated,
                                                                  stream_writer const javascript_output)
{
    for (size_t i = 0; i < generated.count; ++i)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(javascript_output, "else "));
        }
        LPG_TRY(stream_writer_write_string(javascript_output, "if ("));
        LPG_TRY(generate_register_name(generated.key, javascript_output));
        switch (generated.cases[i].kind)
        {
        case match_instruction_case_kind_stateful_enum:
            LPG_UNREACHABLE();

        case match_instruction_case_kind_value:
            LPG_TRY(stream_writer_write_string(javascript_output, " === "));
            LPG_TRY(generate_register_name(generated.cases[i].key_value, javascript_output));
            break;
        }
        LPG_TRY(stream_writer_write_string(javascript_output, ")\n"));
        LPG_TRY(stream_writer_write_string(javascript_output, "{\n"));
        LPG_TRY(generate_sequence(state, generated.cases[i].action, javascript_output));
        LPG_TRY(generate_register_name(generated.result, javascript_output));
        LPG_TRY(stream_writer_write_string(javascript_output, " = "));
        LPG_TRY(generate_register_name(generated.cases[i].value, javascript_output));
        LPG_TRY(stream_writer_write_string(javascript_output, ";\n"));
        LPG_TRY(stream_writer_write_string(javascript_output, "}\n"));
    }
    return success;
}

static success_indicator generate_stateful_enum_match_cases(function_generation *const state,
                                                            match_instruction const generated,
                                                            stream_writer const javascript_output)
{
    for (size_t i = 0; i < generated.count; ++i)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(javascript_output, "else "));
        }
        LPG_TRY(stream_writer_write_string(javascript_output, "if ("));
        switch (generated.cases[i].kind)
        {
        case match_instruction_case_kind_stateful_enum:
            LPG_TRY(stream_writer_write_string(javascript_output, "(typeof "));
            LPG_TRY(generate_register_name(generated.key, javascript_output));
            LPG_TRY(stream_writer_write_string(javascript_output, " !== \"number\") && ("));
            LPG_TRY(generate_register_name(generated.key, javascript_output));
            LPG_TRY(stream_writer_write_string(javascript_output, "[0] === "));
            LPG_TRY(stream_writer_write_integer(
                javascript_output, integer_create(0, generated.cases[i].stateful_enum.element)));
            LPG_TRY(stream_writer_write_string(javascript_output, ")"));
            break;

        case match_instruction_case_kind_value:
            LPG_TRY(generate_register_name(generated.key, javascript_output));
            LPG_TRY(stream_writer_write_string(javascript_output, " === "));
            LPG_TRY(generate_register_name(generated.cases[i].key_value, javascript_output));
            break;
        }
        LPG_TRY(stream_writer_write_string(javascript_output, ")\n"));
        LPG_TRY(stream_writer_write_string(javascript_output, "{\n"));
        switch (generated.cases[i].kind)
        {
        case match_instruction_case_kind_stateful_enum:
            ASSUME(state->registers[generated.key].type_of.kind == type_kind_enumeration);
            LPG_TRY(write_register(state, generated.cases[i].stateful_enum.where,
                                   state->all_enums[state->registers[generated.key].type_of.enum_]
                                       .elements[generated.cases[i].stateful_enum.element]
                                       .state,
                                   javascript_output));
            LPG_TRY(generate_register_name(generated.key, javascript_output));
            LPG_TRY(stream_writer_write_string(javascript_output, "[1];\n"));
            break;

        case match_instruction_case_kind_value:
            break;
        }
        LPG_TRY(generate_sequence(state, generated.cases[i].action, javascript_output));
        LPG_TRY(generate_register_name(generated.result, javascript_output));
        LPG_TRY(stream_writer_write_string(javascript_output, " = "));
        LPG_TRY(generate_register_name(generated.cases[i].value, javascript_output));
        LPG_TRY(stream_writer_write_string(javascript_output, ";\n"));
        LPG_TRY(stream_writer_write_string(javascript_output, "}\n"));
    }
    return success;
}

static success_indicator generate_match(function_generation *const state, match_instruction const generated,
                                        stream_writer const javascript_output)
{
    LPG_TRY(write_register(state, generated.result, generated.result_type, javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, "undefined;\n"));
    type const key_type = state->registers[generated.key].type_of;
    switch (key_type.kind)
    {
    case type_kind_integer_range:
        return generate_equality_comparable_match_cases(state, generated, javascript_output);

    case type_kind_enumeration:
    {
        if (has_stateful_element(state->all_enums[key_type.enum_]))
        {
            return generate_stateful_enum_match_cases(state, generated, javascript_output);
        }
        return generate_equality_comparable_match_cases(state, generated, javascript_output);
    }

    case type_kind_structure:
    case type_kind_function_pointer:
    case type_kind_unit:
    case type_kind_string_ref:
    case type_kind_tuple:
    case type_kind_type:
    case type_kind_enum_constructor:
    case type_kind_lambda:
    case type_kind_interface:
    case type_kind_method_pointer:
        LPG_UNREACHABLE();
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_lambda_with_captures(function_generation *const state,
                                                       lambda_with_captures_instruction const generated,
                                                       stream_writer const javascript_output)
{
    LPG_TRY(write_register(
        state, generated.into, type_from_lambda(lambda_type_create(generated.lambda)), javascript_output));
    LPG_TRY(generate_lambda_value_from_registers(
        state->all_functions, generated.lambda, generated.captures, generated.capture_count, javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, ";\n"));
    return success;
}

static success_indicator generate_get_method(function_generation *const state, get_method_instruction const generated,
                                             stream_writer const javascript_output)
{
    LPG_TRY(write_register(state, generated.into,
                           type_from_method_pointer(method_pointer_type_create(generated.interface_, generated.method)),
                           javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, "function ("));
    struct method_description const method = state->all_interfaces[generated.interface_].methods[generated.method];
    for (register_id i = 0; i < method.parameters.length; ++i)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(javascript_output, ", "));
        }
        LPG_TRY(generate_register_name(i, javascript_output));
    }
    LPG_TRY(stream_writer_write_string(javascript_output, ") { return "));
    LPG_TRY(generate_register_name(generated.from, javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, ".call_method_"));
    LPG_TRY(stream_writer_write_integer(javascript_output, integer_create(0, generated.method)));
    LPG_TRY(stream_writer_write_string(javascript_output, "("));
    for (register_id i = 0; i < method.parameters.length; ++i)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(javascript_output, ", "));
        }
        LPG_TRY(generate_register_name(i, javascript_output));
    }
    LPG_TRY(stream_writer_write_string(javascript_output, "); };\n"));
    return success;
}

static success_indicator generate_erase_type(function_generation *const state, erase_type_instruction const generated,
                                             stream_writer const javascript_output)
{
    LPG_TRY(write_register(state, generated.into, type_from_interface(generated.impl.target), javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, "new "));
    LPG_TRY(
        generate_implementation_name(generated.impl.target, generated.impl.implementation_index, javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, "("));
    LPG_TRY(generate_register_name(generated.self, javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, ");\n"));
    return success;
}

static success_indicator generate_instruction(function_generation *const state, instruction const generated,
                                              stream_writer const javascript_output)
{
    switch (generated.type)
    {
    case instruction_get_method:
        return generate_get_method(state, generated.get_method, javascript_output);

    case instruction_call:
        return generate_call(state, generated.call, javascript_output);

    case instruction_return:
        LPG_TO_DO();

    case instruction_loop:
        return generate_loop(state, generated.loop, javascript_output);

    case instruction_global:
        ASSUME(state->registers[generated.global_into].kind == register_type_none);
        state->registers[generated.global_into] = register_info_create(register_type_global, type_from_struct(0));
        return success;

    case instruction_read_struct:
        return generate_read_struct(state, generated.read_struct, javascript_output);

    case instruction_break:
        LPG_TRY(write_register(state, generated.break_into, type_from_unit(), javascript_output));
        LPG_TRY(stream_writer_write_string(javascript_output, "undefined;\n"));
        return stream_writer_write_string(javascript_output, "break;\n");

    case instruction_literal:
        return generate_literal(
            state, generated.literal, state->all_functions, state->all_interfaces, javascript_output);

    case instruction_tuple:
        return generate_tuple(state, generated.tuple_, javascript_output);

    case instruction_instantiate_struct:
        return generate_instantiate_struct(state, generated.instantiate_struct, javascript_output);

    case instruction_enum_construct:
        return generate_enum_construct(state, generated.enum_construct, javascript_output);

    case instruction_match:
        return generate_match(state, generated.match, javascript_output);

    case instruction_get_captures:
        ASSUME(state->registers[generated.global_into].kind == register_type_none);
        state->registers[generated.global_into] = register_info_create(
            register_type_captures, type_from_tuple_type(state->current_function->signature->captures));
        return success;

    case instruction_lambda_with_captures:
        return generate_lambda_with_captures(state, generated.lambda_with_captures, javascript_output);

    case instruction_erase_type:
        return generate_erase_type(state, generated.erase_type, javascript_output);
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_sequence(function_generation *const state, instruction_sequence const body,
                                           stream_writer const javascript_output)
{
    for (size_t i = 0; i < body.length; ++i)
    {
        LPG_TRY(generate_instruction(state, body.elements[i], javascript_output));
    }
    return success;
}

static success_indicator generate_function_body(checked_function const function,
                                                checked_function const *const all_functions,
                                                interface const *const all_interfaces, enumeration const *all_enums,
                                                structure const *all_structs, stream_writer const javascript_output)
{
    function_generation state =
        function_generation_create(allocate_array(function.number_of_registers, sizeof(*state.registers)),
                                   all_functions, all_interfaces, all_enums, all_structs, &function);
    for (register_id i = 0; i < function.number_of_registers; ++i)
    {
        state.registers[i] = register_info_create(register_type_none, type_from_unit());
    }
    ASSUME(function.number_of_registers >= (function.signature->self.is_set + function.signature->parameters.length));
    if (function.signature->self.is_set)
    {
        state.registers[0] = register_info_create(register_type_variable, function.signature->self.value);
    }
    for (register_id i = 0; i < function.signature->parameters.length; ++i)
    {
        state.registers[function.signature->self.is_set + i] =
            register_info_create(register_type_variable, function.signature->parameters.elements[i]);
    }
    success_indicator const result = generate_sequence(&state, function.body, javascript_output);
    deallocate(state.registers);
    LPG_TRY(stream_writer_write_string(javascript_output, "return "));
    LPG_TRY(generate_register_name(function.return_value, javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, ";\n"));
    return result;
}

static success_indicator generate_argument_list(size_t const length, stream_writer const javascript_output)
{
    bool comma = false;
    for (register_id i = 0; i < length; ++i)
    {
        if (comma)
        {
            LPG_TRY(stream_writer_write_string(javascript_output, ", "));
        }
        else
        {
            comma = true;
        }
        LPG_TRY(generate_register_name(i, javascript_output));
    }
    return success;
}

static success_indicator define_function(function_id const id, checked_function const function,
                                         checked_function const *const all_functions,
                                         interface const *const all_interfaces, enumeration const *all_enums,
                                         structure const *all_structs, stream_writer const javascript_output)
{
    LPG_TRY(generate_function_name(id, javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, " = function ("));
    bool comma = false;
    for (register_id i = 0; i < function.signature->captures.length; ++i)
    {
        if (comma)
        {
            LPG_TRY(stream_writer_write_string(javascript_output, ", "));
        }
        else
        {
            comma = true;
        }
        LPG_TRY(generate_capture_name(i, javascript_output));
    }
    size_t const total_parameters = (function.signature->parameters.length + function.signature->self.is_set);
    if (total_parameters > 0)
    {
        if (comma)
        {
            LPG_TRY(stream_writer_write_string(javascript_output, ", "));
        }
        LPG_TRY(generate_argument_list(total_parameters, javascript_output));
    }
    LPG_TRY(stream_writer_write_string(javascript_output, ")\n"));
    LPG_TRY(stream_writer_write_string(javascript_output, "{\n"));
    LPG_TRY(generate_function_body(function, all_functions, all_interfaces, all_enums, all_structs, javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, "};\n"));
    return success;
}

static success_indicator define_implementation(LPG_NON_NULL(interface const *const implemented_interface),
                                               interface_id const implemented_id, size_t const implementation_index,
                                               implementation const defined, stream_writer const javascript_output)
{
    LPG_TRY(stream_writer_write_string(javascript_output, "var "));
    LPG_TRY(generate_implementation_name(implemented_id, implementation_index, javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, " = function (self) {\n"));
    LPG_TRY(stream_writer_write_string(javascript_output, "    this.self = self;\n"));
    LPG_TRY(stream_writer_write_string(javascript_output, "};\n"));

    for (size_t i = 0; i < defined.method_count; ++i)
    {
        function_pointer_value const current_method = defined.methods[i];
        size_t const parameter_count = implemented_interface->methods[i].parameters.length;
        ASSUME(!current_method.external);
        LPG_TRY(generate_implementation_name(implemented_id, implementation_index, javascript_output));
        LPG_TRY(stream_writer_write_string(javascript_output, ".prototype.call_method_"));
        LPG_TRY(stream_writer_write_integer(javascript_output, integer_create(0, i)));
        LPG_TRY(stream_writer_write_string(javascript_output, " = function ("));
        LPG_TRY(generate_argument_list(parameter_count, javascript_output));
        LPG_TRY(stream_writer_write_string(javascript_output, ") {\n"));
        LPG_TRY(stream_writer_write_string(javascript_output, "    return "));
        LPG_TRY(generate_function_name(current_method.code, javascript_output));
        LPG_TRY(stream_writer_write_string(javascript_output, "(this.self"));
        if (parameter_count > 0)
        {
            LPG_TRY(stream_writer_write_string(javascript_output, ", "));
        }
        LPG_TRY(generate_argument_list(parameter_count, javascript_output));
        LPG_TRY(stream_writer_write_string(javascript_output, ");\n"));
        LPG_TRY(stream_writer_write_string(javascript_output, "};\n"));
    }
    return success;
}

static success_indicator define_interface(interface_id const id, interface const defined,
                                          stream_writer const javascript_output)
{
    for (size_t i = 0; i < defined.implementation_count; ++i)
    {
        LPG_TRY(define_implementation(&defined, id, i, defined.implementations[i].target, javascript_output));
    }
    return success;
}

success_indicator generate_javascript(checked_program const program, stream_writer const javascript_output)
{
    LPG_TRY(stream_writer_write_string(javascript_output, "(function ()\n"));
    LPG_TRY(stream_writer_write_string(javascript_output, "{\n"));
    LPG_TRY(stream_writer_write_string(javascript_output, "\"use strict\";\n"));
    LPG_TRY(stream_writer_write_string(
        javascript_output, "var string_equals = function (left, right) { return (left === right) ? 1.0 : 0.0; };\n"));
    LPG_TRY(stream_writer_write_string(
        javascript_output, "var integer_equals = function (left, right) { return (left === right) ? 1.0 : 0.0; };\n"));
    LPG_TRY(stream_writer_write_string(
        javascript_output, "var integer_less = function (left, right) { return (left < right) ? 1.0 : 0.0; };\n"));
    LPG_TRY(stream_writer_write_string(
        javascript_output, "var concat = function (left, right) { return (left + right); };\n"));
    LPG_TRY(stream_writer_write_string(
        javascript_output,
        "var or = function (left, right) { return ((left === 1.0) || (right === 1.0)) ? 1.0 : 0.0; };\n"));
    LPG_TRY(stream_writer_write_string(
        javascript_output,
        "var and = function (left, right) { return ((left === 1.0) && (right === 1.0)) ? 1.0 : 0.0; };\n"));
    LPG_TRY(stream_writer_write_string(
        javascript_output, "var not = function (argument) { return ((argument === 1.0) ? 0.0 : 1.0); };\n"));
    LPG_TRY(stream_writer_write_string(javascript_output, "var side_effect = function () {};\n"));
    LPG_TRY(stream_writer_write_string(
        javascript_output, "var integer_to_string = function (input) { return \"\" + input; };\n"));
    for (interface_id i = 0; i < program.interface_count; ++i)
    {
        LPG_TRY(define_interface(i, program.interfaces[i], javascript_output));
    }
    for (function_id i = 1; i < program.function_count; ++i)
    {
        LPG_TRY(declare_function(i, javascript_output));
    }
    for (function_id i = 1; i < program.function_count; ++i)
    {
        LPG_TRY(define_function(i, program.functions[i], program.functions, program.interfaces, program.enums,
                                program.structs, javascript_output));
    }
    ASSUME(program.functions[0].signature->parameters.length == 0);
    LPG_TRY(generate_function_body(program.functions[0], program.functions, program.interfaces, program.enums,
                                   program.structs, javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, "})();\n"));
    return success;
}
