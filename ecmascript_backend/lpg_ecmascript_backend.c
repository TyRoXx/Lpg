#include "lpg_ecmascript_backend.h"
#include "lpg_assert.h"
#include "lpg_instruction.h"
#include "lpg_allocate.h"
#include "lpg_structure_member.h"

static success_indicator generate_function_name(function_id const id, function_id const function_count,
                                                stream_writer const ecmascript_output)
{
    ASSUME(id < function_count);
    LPG_TRY(stream_writer_write_string(ecmascript_output, "lambda_"));
    LPG_TRY(stream_writer_write_integer(ecmascript_output, integer_create(0, id)));
    return success_yes;
}

static success_indicator declare_function(function_id const id, function_id const function_count,
                                          stream_writer const ecmascript_output)
{
    LPG_TRY(stream_writer_write_string(ecmascript_output, "var "));
    LPG_TRY(generate_function_name(id, function_count, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, ";\n"));
    return success_yes;
}

static success_indicator generate_register_name(register_id const id, stream_writer const ecmascript_output)
{
    LPG_TRY(stream_writer_write_string(ecmascript_output, "r_"));
    LPG_TRY(stream_writer_write_integer(ecmascript_output, integer_create(0, id)));
    return success_yes;
}

static success_indicator encode_string_literal(unicode_view const content, stream_writer const ecmascript_output)
{
    LPG_TRY(stream_writer_write_string(ecmascript_output, "\""));
    for (size_t i = 0; i < content.length; ++i)
    {
        switch (content.begin[i])
        {
        case '\n':
            LPG_TRY(stream_writer_write_string(ecmascript_output, "\\n"));
            break;

        default:
            LPG_TRY(stream_writer_write_bytes(ecmascript_output, content.begin + i, 1));
        }
    }
    LPG_TRY(stream_writer_write_string(ecmascript_output, "\""));
    return success_yes;
}

static success_indicator generate_value(value const generated, type const type_of,
                                        checked_function const *all_functions, function_id const function_count,
                                        lpg_interface const *const all_interfaces, structure const *const all_structs,
                                        stream_writer const ecmascript_output);

static success_indicator generate_enum_element(enum_element_value const element, checked_function const *all_functions,
                                               function_id const function_count,
                                               lpg_interface const *const all_interfaces,
                                               structure const *const all_structs,
                                               stream_writer const ecmascript_output)
{
    if (element.state)
    {
        LPG_TRY(stream_writer_write_string(ecmascript_output, "["));
        LPG_TRY(stream_writer_write_integer(ecmascript_output, integer_create(0, element.which)));
        LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
        LPG_TRY(generate_value(*element.state, element.state_type, all_functions, function_count, all_interfaces,
                               all_structs, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "]"));
        return success_yes;
    }
    return stream_writer_write_integer(ecmascript_output, integer_create(0, element.which));
}

static success_indicator generate_enum_constructor(enum_constructor_type const constructor,
                                                   stream_writer const ecmascript_output)
{
    LPG_TRY(stream_writer_write_string(ecmascript_output, "function (state) { return ["));
    LPG_TRY(stream_writer_write_integer(ecmascript_output, integer_create(0, constructor.which)));
    LPG_TRY(stream_writer_write_string(ecmascript_output, ", state]; }"));
    return success_yes;
}

static success_indicator generate_implementation_name(interface_id const interface_, size_t const implementation_index,
                                                      stream_writer const ecmascript_output)
{
    LPG_TRY(stream_writer_write_string(ecmascript_output, "impl_"));
    LPG_TRY(stream_writer_write_integer(ecmascript_output, integer_create(0, interface_)));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "_"));
    LPG_TRY(stream_writer_write_integer(ecmascript_output, integer_create(0, implementation_index)));
    return success_yes;
}

static success_indicator generate_capture_alias(size_t const index, stream_writer const ecmascript_output)
{
    LPG_TRY(stream_writer_write_string(ecmascript_output, "capture_"));
    return stream_writer_write_integer(ecmascript_output, integer_create(0, index));
}

static success_indicator
generate_lambda_value_from_registers(checked_function const *all_functions, function_id const function_count,
                                     function_id const lambda, register_id const *const captures,
                                     size_t const capture_count, stream_writer const ecmascript_output)
{
    LPG_TRY(stream_writer_write_string(ecmascript_output, "(function () { "));
    for (register_id i = 0; i < capture_count; ++i)
    {
        LPG_TRY(stream_writer_write_string(ecmascript_output, "var "));
        LPG_TRY(generate_capture_alias(i, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, " = "));
        LPG_TRY(generate_register_name(captures[i], ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, ";\n"));
    }
    LPG_TRY(stream_writer_write_string(ecmascript_output, "return function ("));
    checked_function const *const function = &all_functions[lambda];
    for (register_id i = 0; i < function->signature->parameters.length; ++i)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
        }
        LPG_TRY(generate_register_name(i, ecmascript_output));
    }
    LPG_TRY(stream_writer_write_string(ecmascript_output, ") { return "));
    LPG_TRY(generate_function_name(lambda, function_count, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "("));
    bool comma = false;
    for (register_id i = 0; i < capture_count; ++i)
    {
        if (comma)
        {
            LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
        }
        else
        {
            comma = true;
        }
        LPG_TRY(generate_capture_alias(i, ecmascript_output));
    }
    ASSUME(comma);
    for (register_id i = 0; i < function->signature->parameters.length; ++i)
    {
        LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
        LPG_TRY(generate_register_name(i, ecmascript_output));
    }
    LPG_TRY(stream_writer_write_string(ecmascript_output, "); }; })()"));
    return success_yes;
}

static success_indicator generate_lambda_value_from_values(checked_function const *all_functions,
                                                           function_id const function_count,
                                                           lpg_interface const *const all_interfaces,
                                                           structure const *const all_structs, function_id const lambda,
                                                           value const *const captures, size_t const capture_count,
                                                           stream_writer const ecmascript_output)
{
    LPG_TRY(stream_writer_write_string(ecmascript_output, "function ("));
    checked_function const *const function = &all_functions[lambda];
    for (register_id i = 0; i < function->signature->parameters.length; ++i)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
        }
        LPG_TRY(generate_register_name(i, ecmascript_output));
    }
    LPG_TRY(stream_writer_write_string(ecmascript_output, ") { return "));
    LPG_TRY(generate_function_name(lambda, function_count, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "("));
    bool comma = false;
    for (register_id i = 0; i < capture_count; ++i)
    {
        if (comma)
        {
            LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
        }
        else
        {
            comma = true;
        }
        LPG_TRY(generate_value(captures[i], function->signature->captures.elements[i], all_functions, function_count,
                               all_interfaces, all_structs, ecmascript_output));
    }
    ASSUME(comma);
    for (register_id i = 0; i < function->signature->parameters.length; ++i)
    {
        LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
        LPG_TRY(generate_register_name(i, ecmascript_output));
    }
    LPG_TRY(stream_writer_write_string(ecmascript_output, "); }"));
    return success_yes;
}

static success_indicator generate_value(value const generated, type const type_of,
                                        checked_function const *all_functions, function_id const function_count,
                                        lpg_interface const *const all_interfaces, structure const *const all_structs,
                                        stream_writer const ecmascript_output)
{
    switch (generated.kind)
    {
    case value_kind_type_erased:
        LPG_TRY(stream_writer_write_string(ecmascript_output, "new "));
        LPG_TRY(generate_implementation_name(
            generated.type_erased.impl.target, generated.type_erased.impl.implementation_index, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "("));
        LPG_TRY(generate_value(*generated.type_erased.self,
                               all_interfaces[generated.type_erased.impl.target]
                                   .implementations[generated.type_erased.impl.implementation_index]
                                   .self,
                               all_functions, function_count, all_interfaces, all_structs, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, ")"));
        return success_yes;

    case value_kind_integer:
        if (integer_less(integer_create(0, UINT32_MAX), generated.integer_))
        {
            LPG_TO_DO();
        }
        return stream_writer_write_integer(ecmascript_output, generated.integer_);

    case value_kind_string:
        return encode_string_literal(generated.string_ref, ecmascript_output);

    case value_kind_function_pointer:
        ASSUME(!generated.function_pointer.external);
        if (generated.function_pointer.capture_count > 0)
        {
            return generate_lambda_value_from_values(
                all_functions, function_count, all_interfaces, all_structs, generated.function_pointer.code,
                generated.function_pointer.captures, generated.function_pointer.capture_count, ecmascript_output);
        }
        return generate_function_name(generated.function_pointer.code, function_count, ecmascript_output);

    case value_kind_pattern:
        LPG_TO_DO();

    case value_kind_generic_enum:
        return stream_writer_write_string(ecmascript_output, "/*generic enum*/ undefined");

    case value_kind_type:
        return stream_writer_write_string(ecmascript_output, "/*TODO type*/ undefined");

    case value_kind_enum_element:
        return generate_enum_element(
            generated.enum_element, all_functions, function_count, all_interfaces, all_structs, ecmascript_output);

    case value_kind_unit:
        return stream_writer_write_string(ecmascript_output, "undefined");

    case value_kind_structure:
    {
        LPG_TRY(stream_writer_write_string(ecmascript_output, "["));
        ASSUME(type_of.kind == type_kind_structure);
        structure const object = all_structs[type_of.structure_];
        ASSUME(generated.structure.count == object.count);
        for (size_t i = 0; i < object.count; ++i)
        {
            if (i > 0)
            {
                LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
            }
            LPG_TRY(generate_value(generated.structure.members[i], object.members[i].what, all_functions,
                                   function_count, all_interfaces, all_structs, ecmascript_output));
        }
        LPG_TRY(stream_writer_write_string(ecmascript_output, "]"));
        return success_yes;
    }

    case value_kind_tuple:
    {
        LPG_TRY(stream_writer_write_string(ecmascript_output, "["));
        ASSUME(type_of.kind == type_kind_tuple);
        ASSUME(generated.tuple_.element_count == type_of.tuple_.length);
        for (size_t i = 0; i < generated.tuple_.element_count; ++i)
        {
            if (i > 0)
            {
                LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
            }
            LPG_TRY(generate_value(generated.tuple_.elements[i], type_of.tuple_.elements[i], all_functions,
                                   function_count, all_interfaces, all_structs, ecmascript_output));
        }
        LPG_TRY(stream_writer_write_string(ecmascript_output, "]"));
        return success_yes;
    }

    case value_kind_enum_constructor:
        return generate_enum_constructor(*type_of.enum_constructor, ecmascript_output);
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_var(register_id const id, stream_writer const ecmascript_output)
{
    LPG_TRY(stream_writer_write_string(ecmascript_output, "var "));
    LPG_TRY(generate_register_name(id, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, " = "));
    return success_yes;
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
    function_id function_count;
    lpg_interface const *all_interfaces;
    enumeration const *all_enums;
    structure const *all_structs;
    checked_function const *current_function;
} function_generation;

static function_generation function_generation_create(register_info *registers, checked_function const *all_functions,
                                                      function_id const function_count,
                                                      lpg_interface const *all_interfaces, enumeration const *all_enums,
                                                      structure const *all_structs,
                                                      checked_function const *current_function)
{
    function_generation const result = {
        registers, all_functions, function_count, all_interfaces, all_enums, all_structs, current_function};
    return result;
}

static success_indicator write_register(function_generation *const state, register_id const which, type const type_of,
                                        stream_writer const ecmascript_output)
{
    ASSUME(which < state->current_function->number_of_registers);
    ASSUME(state->registers[which].kind == register_type_none);
    state->registers[which] = register_info_create(register_type_variable, type_of);
    LPG_TRY(generate_var(which, ecmascript_output));
    return success_yes;
}

static success_indicator generate_literal(function_generation *const state, literal_instruction const generated,
                                          checked_function const *all_functions, function_id const function_count,
                                          lpg_interface const *const all_interfaces, structure const *const all_structs,
                                          stream_writer const ecmascript_output)
{
    LPG_TRY(write_register(state, generated.into, generated.type_of, ecmascript_output));
    LPG_TRY(generate_value(generated.value_, generated.type_of, all_functions, function_count, all_interfaces,
                           all_structs, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, ";\n"));
    return success_yes;
}

static success_indicator generate_capture_name(register_id const index, stream_writer const ecmascript_output)
{
    LPG_TRY(stream_writer_write_string(ecmascript_output, "c_"));
    LPG_TRY(stream_writer_write_integer(ecmascript_output, integer_create(0, index)));
    return success_yes;
}

static success_indicator generate_read_struct_value(function_generation *const state,
                                                    read_struct_instruction const generated,
                                                    stream_writer const ecmascript_output)
{
    switch (state->registers[generated.from_object].kind)
    {
    case register_type_none:
        LPG_UNREACHABLE();

    case register_type_variable:
        LPG_TRY(generate_register_name(generated.from_object, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "["));
        LPG_TRY(stream_writer_write_integer(ecmascript_output, integer_create(0, generated.member)));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "]"));
        return success_yes;

    case register_type_captures:
        LPG_TRY(generate_capture_name(generated.member, ecmascript_output));
        return success_yes;

    case register_type_global:
        switch (generated.member)
        {
        case 0:
            return stream_writer_write_string(ecmascript_output, "side_effect");

        case 1:
            return stream_writer_write_string(ecmascript_output, "integer_to_string");

        case 4:
            return stream_writer_write_string(ecmascript_output, "assert");

        case 5:
            return stream_writer_write_string(ecmascript_output, "integer_less");

        case 7:
            return stream_writer_write_string(ecmascript_output, "not");

        case 8:
            return stream_writer_write_string(ecmascript_output, "concat");

        case 9:
            return stream_writer_write_string(ecmascript_output, "string_equals");

        case 12:
            return stream_writer_write_string(ecmascript_output, "integer_equals");

        case 13:
            return stream_writer_write_string(ecmascript_output, "undefined");

        case 14:
            return stream_writer_write_string(ecmascript_output, "undefined");

        default:
            LPG_TO_DO();
        }
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_read_struct(function_generation *const state, read_struct_instruction const generated,
                                              stream_writer const ecmascript_output)
{
    switch (state->registers[generated.from_object].type_of.kind)
    {
    case type_kind_structure:
    {
        struct_id const read_from = state->registers[generated.from_object].type_of.structure_;
        LPG_TRY(write_register(
            state, generated.into, state->all_structs[read_from].members[generated.member].what, ecmascript_output));
        break;
    }

    case type_kind_tuple:
    {
        tuple_type const tuple_ = state->registers[generated.from_object].type_of.tuple_;
        LPG_TRY(write_register(state, generated.into, tuple_.elements[generated.member], ecmascript_output));
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
    case type_kind_generic_enum:
        LPG_UNREACHABLE();
    }
    LPG_TRY(generate_read_struct_value(state, generated, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, ";\n"));
    return success_yes;
}

static success_indicator generate_call(function_generation *const state, call_instruction const generated,
                                       stream_writer const ecmascript_output)
{
    LPG_TRY(write_register(state, generated.result, get_return_type(state->registers[generated.callee].type_of,
                                                                    state->all_functions, state->all_interfaces),
                           ecmascript_output));
    LPG_TRY(generate_register_name(generated.callee, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "("));
    for (size_t i = 0; i < generated.argument_count; ++i)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
        }
        LPG_TRY(generate_register_name(generated.arguments[i], ecmascript_output));
    }
    LPG_TRY(stream_writer_write_string(ecmascript_output, ");\n"));
    return success_yes;
}

static success_indicator generate_sequence(function_generation *const state, instruction_sequence const body,
                                           stream_writer const ecmascript_output);

static success_indicator generate_loop(function_generation *const state, loop_instruction const generated,
                                       stream_writer const ecmascript_output)
{
    LPG_TRY(stream_writer_write_string(ecmascript_output, "for (;;)\n"));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "{\n"));
    LPG_TRY(generate_sequence(state, generated.body, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "}\n"));
    LPG_TRY(write_register(state, generated.unit_goes_into, type_from_unit(), ecmascript_output));
    LPG_TRY(generate_value(value_from_unit(), type_from_unit(), state->all_functions, state->function_count,
                           state->all_interfaces, state->all_structs, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, ";\n"));
    return success_yes;
}

static success_indicator generate_enum_construct(function_generation *const state,
                                                 enum_construct_instruction const construct,
                                                 stream_writer const ecmascript_output)
{
    LPG_TRY(
        write_register(state, construct.into, type_from_enumeration(construct.which.enumeration), ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "["));
    LPG_TRY(stream_writer_write_integer(ecmascript_output, integer_create(0, construct.which.which)));
    LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
    LPG_TRY(generate_register_name(construct.state, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "];\n"));
    return success_yes;
}

static success_indicator generate_tuple(function_generation *const state, tuple_instruction const generated,
                                        stream_writer const ecmascript_output)
{
    LPG_TRY(write_register(state, generated.result, type_from_tuple_type(generated.result_type), ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "["));
    for (size_t i = 0; i < generated.element_count; ++i)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
        }
        LPG_TRY(generate_register_name(generated.elements[i], ecmascript_output));
    }
    LPG_TRY(stream_writer_write_string(ecmascript_output, "];\n"));
    return success_yes;
}

static success_indicator generate_instantiate_struct(function_generation *const state,
                                                     instantiate_struct_instruction const generated,
                                                     stream_writer const ecmascript_output)
{
    LPG_TRY(write_register(state, generated.into, type_from_struct(generated.instantiated), ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "["));
    for (size_t i = 0; i < generated.argument_count; ++i)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
        }
        LPG_TRY(generate_register_name(generated.arguments[i], ecmascript_output));
    }
    LPG_TRY(stream_writer_write_string(ecmascript_output, "];\n"));
    return success_yes;
}

static success_indicator generate_equality_comparable_match_cases(function_generation *const state,
                                                                  match_instruction const generated,
                                                                  stream_writer const ecmascript_output)
{
    for (size_t i = 0; i < generated.count; ++i)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(ecmascript_output, "else "));
        }
        LPG_TRY(stream_writer_write_string(ecmascript_output, "if ("));
        LPG_TRY(generate_register_name(generated.key, ecmascript_output));
        switch (generated.cases[i].kind)
        {
        case match_instruction_case_kind_stateful_enum:
            LPG_UNREACHABLE();

        case match_instruction_case_kind_value:
            LPG_TRY(stream_writer_write_string(ecmascript_output, " === "));
            LPG_TRY(generate_register_name(generated.cases[i].key_value, ecmascript_output));
            break;
        }
        LPG_TRY(stream_writer_write_string(ecmascript_output, ")\n"));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "{\n"));
        LPG_TRY(generate_sequence(state, generated.cases[i].action, ecmascript_output));
        LPG_TRY(generate_register_name(generated.result, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, " = "));
        LPG_TRY(generate_register_name(generated.cases[i].value, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, ";\n"));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "}\n"));
    }
    return success_yes;
}

static success_indicator generate_stateful_enum_match_cases(function_generation *const state,
                                                            match_instruction const generated,
                                                            stream_writer const ecmascript_output)
{
    for (size_t i = 0; i < generated.count; ++i)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(ecmascript_output, "else "));
        }
        LPG_TRY(stream_writer_write_string(ecmascript_output, "if ("));
        switch (generated.cases[i].kind)
        {
        case match_instruction_case_kind_stateful_enum:
            LPG_TRY(stream_writer_write_string(ecmascript_output, "(typeof "));
            LPG_TRY(generate_register_name(generated.key, ecmascript_output));
            LPG_TRY(stream_writer_write_string(ecmascript_output, " !== \"number\") && ("));
            LPG_TRY(generate_register_name(generated.key, ecmascript_output));
            LPG_TRY(stream_writer_write_string(ecmascript_output, "[0] === "));
            LPG_TRY(stream_writer_write_integer(
                ecmascript_output, integer_create(0, generated.cases[i].stateful_enum.element)));
            LPG_TRY(stream_writer_write_string(ecmascript_output, ")"));
            break;

        case match_instruction_case_kind_value:
            LPG_TRY(generate_register_name(generated.key, ecmascript_output));
            LPG_TRY(stream_writer_write_string(ecmascript_output, " === "));
            LPG_TRY(generate_register_name(generated.cases[i].key_value, ecmascript_output));
            break;
        }
        LPG_TRY(stream_writer_write_string(ecmascript_output, ")\n"));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "{\n"));
        switch (generated.cases[i].kind)
        {
        case match_instruction_case_kind_stateful_enum:
            ASSUME(state->registers[generated.key].type_of.kind == type_kind_enumeration);
            LPG_TRY(write_register(state, generated.cases[i].stateful_enum.where,
                                   state->all_enums[state->registers[generated.key].type_of.enum_]
                                       .elements[generated.cases[i].stateful_enum.element]
                                       .state,
                                   ecmascript_output));
            LPG_TRY(generate_register_name(generated.key, ecmascript_output));
            LPG_TRY(stream_writer_write_string(ecmascript_output, "[1];\n"));
            break;

        case match_instruction_case_kind_value:
            break;
        }
        LPG_TRY(generate_sequence(state, generated.cases[i].action, ecmascript_output));
        LPG_TRY(generate_register_name(generated.result, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, " = "));
        LPG_TRY(generate_register_name(generated.cases[i].value, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, ";\n"));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "}\n"));
    }
    return success_yes;
}

static success_indicator generate_match(function_generation *const state, match_instruction const generated,
                                        stream_writer const ecmascript_output)
{
    LPG_TRY(write_register(state, generated.result, generated.result_type, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "undefined;\n"));
    type const key_type = state->registers[generated.key].type_of;
    switch (key_type.kind)
    {
    case type_kind_integer_range:
        return generate_equality_comparable_match_cases(state, generated, ecmascript_output);

    case type_kind_enumeration:
    {
        if (has_stateful_element(state->all_enums[key_type.enum_]))
        {
            return generate_stateful_enum_match_cases(state, generated, ecmascript_output);
        }
        return generate_equality_comparable_match_cases(state, generated, ecmascript_output);
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
    case type_kind_generic_enum:
        LPG_UNREACHABLE();
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_lambda_with_captures(function_generation *const state,
                                                       lambda_with_captures_instruction const generated,
                                                       stream_writer const ecmascript_output)
{
    LPG_TRY(write_register(
        state, generated.into, type_from_lambda(lambda_type_create(generated.lambda)), ecmascript_output));
    LPG_TRY(generate_lambda_value_from_registers(state->all_functions, state->function_count, generated.lambda,
                                                 generated.captures, generated.capture_count, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, ";\n"));
    return success_yes;
}

static success_indicator generate_get_method(function_generation *const state, get_method_instruction const generated,
                                             stream_writer const ecmascript_output)
{
    LPG_TRY(write_register(state, generated.into,
                           type_from_method_pointer(method_pointer_type_create(generated.interface_, generated.method)),
                           ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "function ("));
    struct method_description const method = state->all_interfaces[generated.interface_].methods[generated.method];
    for (register_id i = 0; i < method.parameters.length; ++i)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
        }
        LPG_TRY(generate_register_name(i, ecmascript_output));
    }
    LPG_TRY(stream_writer_write_string(ecmascript_output, ") { return "));
    LPG_TRY(generate_register_name(generated.from, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, ".call_method_"));
    LPG_TRY(stream_writer_write_integer(ecmascript_output, integer_create(0, generated.method)));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "("));
    for (register_id i = 0; i < method.parameters.length; ++i)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
        }
        LPG_TRY(generate_register_name(i, ecmascript_output));
    }
    LPG_TRY(stream_writer_write_string(ecmascript_output, "); };\n"));
    return success_yes;
}

static success_indicator generate_erase_type(function_generation *const state, erase_type_instruction const generated,
                                             stream_writer const ecmascript_output)
{
    LPG_TRY(write_register(state, generated.into, type_from_interface(generated.impl.target), ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "new "));
    LPG_TRY(
        generate_implementation_name(generated.impl.target, generated.impl.implementation_index, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "("));
    LPG_TRY(generate_register_name(generated.self, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, ");\n"));
    return success_yes;
}

static success_indicator generate_return(return_instruction const generated, stream_writer const ecmascript_output)
{
    LPG_TRY(stream_writer_write_string(ecmascript_output, "return "));
    LPG_TRY(generate_register_name(generated.returned_value, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, ";\n"));
    return success_yes;
}

static success_indicator generate_instruction(function_generation *const state, instruction const generated,
                                              stream_writer const ecmascript_output)
{
    switch (generated.type)
    {
    case instruction_get_method:
        return generate_get_method(state, generated.get_method, ecmascript_output);

    case instruction_call:
        return generate_call(state, generated.call, ecmascript_output);

    case instruction_return:
        return generate_return(generated.return_, ecmascript_output);

    case instruction_loop:
        return generate_loop(state, generated.loop, ecmascript_output);

    case instruction_global:
        ASSUME(state->registers[generated.global_into].kind == register_type_none);
        state->registers[generated.global_into] = register_info_create(register_type_global, type_from_struct(0));
        return success_yes;

    case instruction_read_struct:
        return generate_read_struct(state, generated.read_struct, ecmascript_output);

    case instruction_break:
        LPG_TRY(write_register(state, generated.break_into, type_from_unit(), ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "undefined;\n"));
        return stream_writer_write_string(ecmascript_output, "break;\n");

    case instruction_literal:
        return generate_literal(state, generated.literal, state->all_functions, state->function_count,
                                state->all_interfaces, state->all_structs, ecmascript_output);

    case instruction_tuple:
        return generate_tuple(state, generated.tuple_, ecmascript_output);

    case instruction_instantiate_struct:
        return generate_instantiate_struct(state, generated.instantiate_struct, ecmascript_output);

    case instruction_enum_construct:
        return generate_enum_construct(state, generated.enum_construct, ecmascript_output);

    case instruction_match:
        return generate_match(state, generated.match, ecmascript_output);

    case instruction_get_captures:
        ASSUME(state->registers[generated.global_into].kind == register_type_none);
        state->registers[generated.global_into] = register_info_create(
            register_type_captures, type_from_tuple_type(state->current_function->signature->captures));
        return success_yes;

    case instruction_lambda_with_captures:
        return generate_lambda_with_captures(state, generated.lambda_with_captures, ecmascript_output);

    case instruction_erase_type:
        return generate_erase_type(state, generated.erase_type, ecmascript_output);
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_sequence(function_generation *const state, instruction_sequence const body,
                                           stream_writer const ecmascript_output)
{
    for (size_t i = 0; i < body.length; ++i)
    {
        LPG_TRY(generate_instruction(state, body.elements[i], ecmascript_output));
    }
    return success_yes;
}

static success_indicator generate_function_body(checked_function const function,
                                                checked_function const *const all_functions,
                                                function_id const function_count,
                                                lpg_interface const *const all_interfaces, enumeration const *all_enums,
                                                structure const *all_structs, stream_writer const ecmascript_output)
{
    function_generation state =
        function_generation_create(allocate_array(function.number_of_registers, sizeof(*state.registers)),
                                   all_functions, function_count, all_interfaces, all_enums, all_structs, &function);
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
    success_indicator const result = generate_sequence(&state, function.body, ecmascript_output);
    deallocate(state.registers);
    return result;
}

static success_indicator generate_argument_list(size_t const length, stream_writer const ecmascript_output)
{
    bool comma = false;
    for (register_id i = 0; i < length; ++i)
    {
        if (comma)
        {
            LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
        }
        else
        {
            comma = true;
        }
        LPG_TRY(generate_register_name(i, ecmascript_output));
    }
    return success_yes;
}

static success_indicator define_function(function_id const id, checked_function const function,
                                         checked_function const *const all_functions, function_id const function_count,
                                         lpg_interface const *const all_interfaces, enumeration const *all_enums,
                                         structure const *all_structs, stream_writer const ecmascript_output)
{
    LPG_TRY(generate_function_name(id, function_count, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, " = function ("));
    bool comma = false;
    for (register_id i = 0; i < function.signature->captures.length; ++i)
    {
        if (comma)
        {
            LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
        }
        else
        {
            comma = true;
        }
        LPG_TRY(generate_capture_name(i, ecmascript_output));
    }
    size_t const total_parameters = (function.signature->parameters.length + function.signature->self.is_set);
    if (total_parameters > 0)
    {
        if (comma)
        {
            LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
        }
        LPG_TRY(generate_argument_list(total_parameters, ecmascript_output));
    }
    LPG_TRY(stream_writer_write_string(ecmascript_output, ")\n"));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "{\n"));
    LPG_TRY(generate_function_body(
        function, all_functions, function_count, all_interfaces, all_enums, all_structs, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "};\n"));
    return success_yes;
}

static success_indicator define_implementation(LPG_NON_NULL(lpg_interface const *const implemented_interface),
                                               interface_id const implemented_id, size_t const implementation_index,
                                               implementation const defined, function_id const function_count,
                                               stream_writer const ecmascript_output)
{
    LPG_TRY(stream_writer_write_string(ecmascript_output, "var "));
    LPG_TRY(generate_implementation_name(implemented_id, implementation_index, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, " = function (self) {\n"));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "    this.self = self;\n"));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "};\n"));

    for (size_t i = 0; i < defined.method_count; ++i)
    {
        function_pointer_value const current_method = defined.methods[i];
        size_t const parameter_count = implemented_interface->methods[i].parameters.length;
        ASSUME(!current_method.external);
        LPG_TRY(generate_implementation_name(implemented_id, implementation_index, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, ".prototype.call_method_"));
        LPG_TRY(stream_writer_write_integer(ecmascript_output, integer_create(0, i)));
        LPG_TRY(stream_writer_write_string(ecmascript_output, " = function ("));
        LPG_TRY(generate_argument_list(parameter_count, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, ") {\n"));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "    return "));
        LPG_TRY(generate_function_name(current_method.code, function_count, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "(this.self"));
        if (parameter_count > 0)
        {
            LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
        }
        LPG_TRY(generate_argument_list(parameter_count, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, ");\n"));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "};\n"));
    }
    return success_yes;
}

static success_indicator define_interface(interface_id const id, lpg_interface const defined,
                                          function_id const function_count, stream_writer const ecmascript_output)
{
    for (size_t i = 0; i < defined.implementation_count; ++i)
    {
        LPG_TRY(define_implementation(
            &defined, id, i, defined.implementations[i].target, function_count, ecmascript_output));
    }
    return success_yes;
}

success_indicator generate_ecmascript(checked_program const program, stream_writer const ecmascript_output)
{
    LPG_TRY(stream_writer_write_string(ecmascript_output, "(function ()\n"));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "{\n"));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "\"use strict\";\n"));
    LPG_TRY(stream_writer_write_string(
        ecmascript_output, "var string_equals = function (left, right) { return (left === right) ? 1.0 : 0.0; };\n"));
    LPG_TRY(stream_writer_write_string(
        ecmascript_output, "var integer_equals = function (left, right) { return (left === right) ? 1.0 : 0.0; };\n"));
    LPG_TRY(stream_writer_write_string(
        ecmascript_output, "var integer_less = function (left, right) { return (left < right) ? 1.0 : 0.0; };\n"));
    LPG_TRY(stream_writer_write_string(
        ecmascript_output, "var concat = function (left, right) { return (left + right); };\n"));
    LPG_TRY(stream_writer_write_string(
        ecmascript_output, "var not = function (argument) { return ((argument === 1.0) ? 0.0 : 1.0); };\n"));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "var side_effect = function () {};\n"));
    LPG_TRY(stream_writer_write_string(
        ecmascript_output, "var integer_to_string = function (input) { return \"\" + input; };\n"));
    for (interface_id i = 0; i < program.interface_count; ++i)
    {
        LPG_TRY(define_interface(i, program.interfaces[i], program.function_count, ecmascript_output));
    }
    for (function_id i = 1; i < program.function_count; ++i)
    {
        LPG_TRY(declare_function(i, program.function_count, ecmascript_output));
    }
    for (function_id i = 1; i < program.function_count; ++i)
    {
        LPG_TRY(define_function(i, program.functions[i], program.functions, program.function_count, program.interfaces,
                                program.enums, program.structs, ecmascript_output));
    }
    ASSUME(program.functions[0].signature->parameters.length == 0);
    LPG_TRY(generate_function_body(program.functions[0], program.functions, program.function_count, program.interfaces,
                                   program.enums, program.structs, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "})();\n"));
    return success_yes;
}
