#include "lpg_ecmascript_backend.h"
#include "lpg_allocate.h"
#include "lpg_assert.h"
#include "lpg_enum_encoding.h"
#include "lpg_function_generation.h"
#include "lpg_instruction.h"
#include "lpg_structure_member.h"

static success_indicator indent(size_t const indentation, stream_writer const c_output)
{
    for (size_t i = 0; i < indentation; ++i)
    {
        LPG_TRY(stream_writer_write_string(c_output, "    "));
    }
    return success_yes;
}

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

static success_indicator generate_enum_constructor(enum_constructor_type const constructor,
                                                   stream_writer const ecmascript_output)
{
    LPG_TRY(stream_writer_write_string(ecmascript_output, "function (state) { return "));
    LPG_TRY(enum_construct_stateful_begin(constructor.which, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "state"));
    LPG_TRY(enum_construct_stateful_end(ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "; }"));
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
generate_lambda_value_from_values(checked_function const *all_functions, function_id const function_count,
                                  lpg_interface const *const all_interfaces, structure const *const all_structs,
                                  function_id const lambda, value const *const captures, size_t const capture_count,
                                  enumeration const *const all_enums, stream_writer const ecmascript_output)
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
                               all_interfaces, all_structs, all_enums, ecmascript_output));
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

static success_indicator generate_array_value(array_value const generated, checked_function const *all_functions,
                                              function_id const function_count,
                                              lpg_interface const *const all_interfaces,
                                              structure const *const all_structs, enumeration const *const all_enums,
                                              stream_writer const ecmascript_output)
{
    LPG_TRY(stream_writer_write_string(ecmascript_output, "new new_array(["));
    for (size_t i = 0; i < generated.count; ++i)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
        }
        LPG_TRY(generate_value(generated.elements[i], generated.element_type, all_functions, function_count,
                               all_interfaces, all_structs, all_enums, ecmascript_output));
    }
    LPG_TRY(stream_writer_write_string(ecmascript_output, "])"));
    return success_yes;
}

success_indicator generate_value(value const generated, type const type_of, checked_function const *all_functions,
                                 function_id const function_count, lpg_interface const *const all_interfaces,
                                 structure const *const all_structs, enumeration const *const all_enums,
                                 stream_writer const ecmascript_output)
{
    ASSUME(value_conforms_to_type(generated, type_of));
    ASSUME(value_is_valid(generated));
    switch (generated.kind)
    {
    case value_kind_array:
        return generate_array_value(
            *generated.array, all_functions, function_count, all_interfaces, all_structs, all_enums, ecmascript_output);

    case value_kind_pattern:
        LPG_TO_DO();

    case value_kind_type_erased:
        LPG_TRY(stream_writer_write_string(ecmascript_output, "new "));
        LPG_TRY(generate_implementation_name(
            generated.type_erased.impl.target, generated.type_erased.impl.implementation_index, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "("));
        LPG_TRY(generate_value(
            *generated.type_erased.self, all_interfaces[generated.type_erased.impl.target]
                                             .implementations[generated.type_erased.impl.implementation_index]
                                             .self,
            all_functions, function_count, all_interfaces, all_structs, all_enums, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, ")"));
        return success_yes;

    case value_kind_integer:
    {
        uint64_t const max_ecmascript_int = 9007199254740991;
        if (integer_less(integer_create(0, max_ecmascript_int), generated.integer_))
        {
            LPG_TO_DO();
        }
        return stream_writer_write_integer(ecmascript_output, generated.integer_);
    }

    case value_kind_string:
        return encode_string_literal(generated.string, ecmascript_output);

    case value_kind_function_pointer:
        ASSUME(!generated.function_pointer.external);
        if (generated.function_pointer.capture_count > 0)
        {
            return generate_lambda_value_from_values(
                all_functions, function_count, all_interfaces, all_structs, generated.function_pointer.code,
                generated.function_pointer.captures, generated.function_pointer.capture_count, all_enums,
                ecmascript_output);
        }
        return generate_function_name(generated.function_pointer.code, function_count, ecmascript_output);

    case value_kind_generic_interface:
        return stream_writer_write_string(ecmascript_output, "/*generic interface*/ undefined");

    case value_kind_generic_enum:
        return stream_writer_write_string(ecmascript_output, "/*generic enum*/ undefined");

    case value_kind_generic_struct:
        return stream_writer_write_string(ecmascript_output, "/*generic struct*/ undefined");

    case value_kind_type:
        return stream_writer_write_string(ecmascript_output, "/*TODO type*/ undefined");

    case value_kind_enum_element:
        ASSUME(type_of.kind == type_kind_enumeration);
        return generate_enum_element(generated.enum_element, all_enums + type_of.enum_, all_functions, function_count,
                                     all_interfaces, all_structs, all_enums, ecmascript_output);

    case value_kind_unit:
    case value_kind_generic_lambda:
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
                                   function_count, all_interfaces, all_structs, all_enums, ecmascript_output));
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
                                   function_count, all_interfaces, all_structs, all_enums, ecmascript_output));
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

static success_indicator write_register(function_generation *const state, register_id const which, type const type_of,
                                        optional_value const known_value, stream_writer const ecmascript_output)
{
    ASSUME(which < state->current_function->number_of_registers);
    ASSUME(state->registers[which].kind == register_type_none);
    state->registers[which] = register_info_create(register_type_variable, type_of, known_value);
    LPG_TRY(generate_var(which, ecmascript_output));
    return success_yes;
}

success_indicator generate_register_read(function_generation *const state, register_id const id,
                                         stream_writer const ecmascript_output)
{
    register_info const info = state->registers[id];
    if (!info.known_value.is_set)
    {
        return generate_register_name(id, ecmascript_output);
    }
    switch (info.known_value.value_.kind)
    {
    case value_kind_generic_struct:
    case value_kind_integer:
    case value_kind_string:
    case value_kind_generic_enum:
    case value_kind_generic_interface:
    case value_kind_generic_lambda:
    case value_kind_type:
    case value_kind_unit:
    case value_kind_pattern:
        return generate_value(info.known_value.value_, info.type_of, state->all_functions, state->function_count,
                              state->all_interfaces, state->all_structs, state->all_enums, ecmascript_output);

    case value_kind_function_pointer:
        if (info.known_value.value_.function_pointer.capture_count > 0)
        {
            return generate_register_name(id, ecmascript_output);
        }
        return generate_value(info.known_value.value_, info.type_of, state->all_functions, state->function_count,
                              state->all_interfaces, state->all_structs, state->all_enums, ecmascript_output);

    case value_kind_structure:
    case value_kind_enum_element:
    case value_kind_tuple:
    case value_kind_enum_constructor:
    case value_kind_type_erased:
    case value_kind_array:
        return generate_register_name(id, ecmascript_output);
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_lambda_value_from_registers(function_generation *const state,
                                                              function_id const lambda,
                                                              register_id const *const captures,
                                                              size_t const capture_count, size_t const indentation,
                                                              stream_writer const ecmascript_output)
{
    LPG_TRY(stream_writer_write_string(ecmascript_output, "(function () { "));
    for (register_id i = 0; i < capture_count; ++i)
    {
        LPG_TRY(stream_writer_write_string(ecmascript_output, "\n"));
        LPG_TRY(indent((indentation + 1), ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "var "));
        LPG_TRY(generate_capture_alias(i, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, " = "));
        LPG_TRY(generate_register_read(state, captures[i], ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, ";"));
    }
    LPG_TRY(stream_writer_write_string(ecmascript_output, "\n"));
    LPG_TRY(indent((indentation + 1), ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "return function ("));
    checked_function const *const function = &state->all_functions[lambda];
    for (register_id i = 0; i < function->signature->parameters.length; ++i)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
        }
        LPG_TRY(generate_register_name(i, ecmascript_output));
    }
    LPG_TRY(stream_writer_write_string(ecmascript_output, ") { return "));
    LPG_TRY(generate_function_name(lambda, state->function_count, ecmascript_output));
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
    LPG_TRY(stream_writer_write_string(ecmascript_output, "); };\n"));
    LPG_TRY(indent(indentation, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "})()"));
    return success_yes;
}

static success_indicator generate_literal(function_generation *const state, literal_instruction const generated,
                                          checked_function const *all_functions, function_id const function_count,
                                          lpg_interface const *const all_interfaces, structure const *const all_structs,
                                          const size_t indentation, enumeration const *const all_enums,
                                          stream_writer const ecmascript_output)
{
    LPG_TRY(indent(indentation, ecmascript_output));
    LPG_TRY(write_register(
        state, generated.into, generated.type_of, optional_value_create(generated.value_), ecmascript_output));
    LPG_TRY(generate_value(generated.value_, generated.type_of, all_functions, function_count, all_interfaces,
                           all_structs, all_enums, ecmascript_output));
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
        LPG_TRY(generate_register_read(state, generated.from_object, ecmascript_output));
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

        case 3:
            return stream_writer_write_string(ecmascript_output, "undefined");

        case 4:
            return stream_writer_write_string(ecmascript_output, "assert");

        case 5:
            return stream_writer_write_string(ecmascript_output, "integer_less");

        case 6:
            return stream_writer_write_string(ecmascript_output, "integer_equals");

        case 7:
            return stream_writer_write_string(ecmascript_output, "not");

        case 8:
            return stream_writer_write_string(ecmascript_output, "concat");

        case 9:
            return stream_writer_write_string(ecmascript_output, "string_equals");

        case 11:
            return stream_writer_write_string(ecmascript_output, "undefined");

        case 12:
            return stream_writer_write_string(ecmascript_output, "fail");

        case 14:
            return stream_writer_write_string(ecmascript_output, "integer_subtract");

        case 16:
            return stream_writer_write_string(ecmascript_output, "integer_add");

        case 18:
            return stream_writer_write_string(ecmascript_output, "integer_add_u32");

        default:
            LPG_TO_DO();
        }
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_read_struct(function_generation *const state, read_struct_instruction const generated,
                                              const size_t indentation, stream_writer const ecmascript_output)
{
    LPG_TRY(indent(indentation, ecmascript_output));
    register_info const object = state->registers[generated.from_object];
    ASSUME(object.kind != register_type_none);
    type_kind const kind = object.type_of.kind;
    switch (kind)
    {
    case type_kind_generic_struct:
        LPG_TO_DO();

    case type_kind_host_value:
    case type_kind_generic_lambda:
        LPG_TO_DO();

    case type_kind_structure:
    {
        struct_id const read_from = state->registers[generated.from_object].type_of.structure_;
        LPG_TRY(write_register(state, generated.into, state->all_structs[read_from].members[generated.member].what,
                               optional_value_empty, ecmascript_output));
        break;
    }

    case type_kind_tuple:
    {
        tuple_type const tuple_ = state->registers[generated.from_object].type_of.tuple_;
        ASSUME(generated.member < tuple_.length);
        LPG_TRY(write_register(
            state, generated.into, tuple_.elements[generated.member], optional_value_empty, ecmascript_output));
        break;
    }

    case type_kind_function_pointer:
    case type_kind_unit:
    case type_kind_string:
    case type_kind_enumeration:
    case type_kind_type:
    case type_kind_integer_range:
    case type_kind_enum_constructor:
    case type_kind_lambda:
    case type_kind_interface:
    case type_kind_method_pointer:
    case type_kind_generic_enum:
    case type_kind_generic_interface:
        LPG_UNREACHABLE();
    }
    LPG_TRY(generate_read_struct_value(state, generated, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, ";\n"));
    return success_yes;
}

static success_indicator generate_call(function_generation *const state, call_instruction const generated,
                                       const size_t indentation, stream_writer const ecmascript_output)
{
    LPG_TRY(indent(indentation, ecmascript_output));
    type const callee_type = state->registers[generated.callee].type_of;
    optional_type const return_type = get_return_type(callee_type, state->all_functions, state->all_interfaces);
    if (return_type.is_set)
    {
        LPG_TRY(write_register(state, generated.result, return_type.value, optional_value_empty, ecmascript_output));
    }
    LPG_TRY(generate_register_read(state, generated.callee, ecmascript_output));
    switch (callee_type.kind)
    {
    case type_kind_generic_struct:
        LPG_TO_DO();

    case type_kind_method_pointer:
        LPG_TRY(stream_writer_write_string(ecmascript_output, ".call_method_"));
        LPG_TRY(
            stream_writer_write_integer(ecmascript_output, integer_create(0, callee_type.method_pointer.method_index)));
        break;

    case type_kind_function_pointer:
    case type_kind_enum_constructor:
    case type_kind_lambda:
        break;

    case type_kind_structure:
    case type_kind_unit:
    case type_kind_string:
    case type_kind_enumeration:
    case type_kind_tuple:
    case type_kind_type:
    case type_kind_integer_range:
    case type_kind_interface:
    case type_kind_generic_enum:
    case type_kind_generic_interface:
    case type_kind_generic_lambda:
    case type_kind_host_value:
        LPG_UNREACHABLE();
    }
    LPG_TRY(stream_writer_write_string(ecmascript_output, "("));
    for (size_t i = 0; i < generated.argument_count; ++i)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
        }
        LPG_TRY(generate_register_read(state, generated.arguments[i], ecmascript_output));
    }
    LPG_TRY(stream_writer_write_string(ecmascript_output, ");\n"));
    return success_yes;
}

static success_indicator generate_sequence(function_generation *const state, instruction_sequence const body,
                                           const size_t indentation, stream_writer const ecmascript_output);

static success_indicator generate_loop(function_generation *const state, loop_instruction const generated,
                                       const size_t indentation, stream_writer const ecmascript_output)
{
    LPG_TRY(indent(indentation, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "for (;;)\n"));
    LPG_TRY(indent(indentation, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "{\n"));
    LPG_TRY(generate_sequence(state, generated.body, indentation + 1, ecmascript_output));
    LPG_TRY(indent(indentation, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "}\n"));
    LPG_TRY(indent(indentation, ecmascript_output));
    LPG_TRY(write_register(state, generated.unit_goes_into, type_from_unit(), optional_value_empty, ecmascript_output));
    LPG_TRY(generate_value(value_from_unit(), type_from_unit(), state->all_functions, state->function_count,
                           state->all_interfaces, state->all_structs, state->all_enums, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, ";\n"));
    return success_yes;
}

static success_indicator generate_enum_construct(function_generation *const state,
                                                 enum_construct_instruction const construct, const size_t indentation,
                                                 stream_writer const ecmascript_output)
{
    LPG_TRY(indent(indentation, ecmascript_output));
    LPG_TRY(write_register(state, construct.into, type_from_enumeration(construct.which.enumeration),
                           optional_value_empty, ecmascript_output));
    LPG_TRY(enum_construct_stateful_begin(construct.which.which, ecmascript_output));
    LPG_TRY(generate_register_read(state, construct.state, ecmascript_output));
    LPG_TRY(enum_construct_stateful_end(ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, ";\n"));
    return success_yes;
}

static success_indicator generate_tuple(function_generation *const state, tuple_instruction const generated,
                                        const size_t indentation, stream_writer const ecmascript_output)
{
    LPG_TRY(indent(indentation, ecmascript_output));
    LPG_TRY(write_register(
        state, generated.result, type_from_tuple_type(generated.result_type), optional_value_empty, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "["));
    for (size_t i = 0; i < generated.element_count; ++i)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
        }
        LPG_TRY(generate_register_read(state, generated.elements[i], ecmascript_output));
    }
    LPG_TRY(stream_writer_write_string(ecmascript_output, "];\n"));
    return success_yes;
}

static success_indicator generate_instantiate_struct(function_generation *const state,
                                                     instantiate_struct_instruction const generated,
                                                     const size_t indentation, stream_writer const ecmascript_output)
{
    LPG_TRY(indent(indentation, ecmascript_output));
    LPG_TRY(write_register(
        state, generated.into, type_from_struct(generated.instantiated), optional_value_empty, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "["));
    for (size_t i = 0; i < generated.argument_count; ++i)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
        }
        LPG_TRY(generate_register_read(state, generated.arguments[i], ecmascript_output));
    }
    LPG_TRY(stream_writer_write_string(ecmascript_output, "];\n"));
    return success_yes;
}

static success_indicator generate_equality_comparable_match_cases(function_generation *const state,
                                                                  match_instruction const generated,
                                                                  const size_t indentation,
                                                                  stream_writer const ecmascript_output)
{
    for (size_t i = 0; i < generated.count; ++i)
    {
        LPG_TRY(indent(indentation, ecmascript_output));
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(ecmascript_output, "else "));
        }
        LPG_TRY(stream_writer_write_string(ecmascript_output, "if ("));
        switch (generated.cases[i].kind)
        {
        case match_instruction_case_kind_stateful_enum:
            LPG_UNREACHABLE();

        case match_instruction_case_kind_value:
            break;
        }
        LPG_TRY(
            generate_stateless_enum_case_check(state, generated.key, generated.cases[i].key_value, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, ")\n"));
        LPG_TRY(indent(indentation, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "{\n"));
        LPG_TRY(generate_sequence(state, generated.cases[i].action, indentation + 1, ecmascript_output));

        if (generated.cases[i].value.is_set)
        {
            LPG_TRY(indent(indentation + 1, ecmascript_output));
            LPG_TRY(generate_register_name(generated.result, ecmascript_output));
            LPG_TRY(stream_writer_write_string(ecmascript_output, " = "));
            LPG_TRY(generate_register_read(state, generated.cases[i].value.value, ecmascript_output));
            LPG_TRY(stream_writer_write_string(ecmascript_output, ";\n"));
        }
        LPG_TRY(indent(indentation, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "}\n"));
    }
    return success_yes;
}

static success_indicator generate_stateful_enum_match_cases(function_generation *const state,
                                                            match_instruction const generated, const size_t indentation,
                                                            stream_writer const ecmascript_output)
{
    for (size_t i = 0; i < generated.count; ++i)
    {
        LPG_TRY(indent(indentation, ecmascript_output));
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(ecmascript_output, "else "));
        }
        LPG_TRY(stream_writer_write_string(ecmascript_output, "if ("));
        switch (generated.cases[i].kind)
        {
        case match_instruction_case_kind_stateful_enum:
        {
            LPG_TRY(stateful_enum_case_check(
                state, generated.key, generated.cases[i].stateful_enum.element, ecmascript_output));
            break;
        }

        case match_instruction_case_kind_value:
            LPG_TRY(generate_stateless_enum_case_check(
                state, generated.key, generated.cases[i].key_value, ecmascript_output));
            break;
        }
        LPG_TRY(stream_writer_write_string(ecmascript_output, ")\n"));
        LPG_TRY(indent(indentation, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "{\n"));

        switch (generated.cases[i].kind)
        {
        case match_instruction_case_kind_stateful_enum:
        {
            LPG_TRY(indent(indentation + 1, ecmascript_output));
            ASSUME(state->registers[generated.key].type_of.kind == type_kind_enumeration);
            optional_type const enum_state = state->all_enums[state->registers[generated.key].type_of.enum_]
                                                 .elements[generated.cases[i].stateful_enum.element]
                                                 .state;
            ASSUME(enum_state.is_set);
            LPG_TRY(write_register(state, generated.cases[i].stateful_enum.where, enum_state.value,
                                   optional_value_empty, ecmascript_output));
            LPG_TRY(generate_register_read(state, generated.key, ecmascript_output));
            LPG_TRY(stateful_enum_get_state(ecmascript_output));
            LPG_TRY(stream_writer_write_string(ecmascript_output, ";\n"));
            break;
        }

        case match_instruction_case_kind_value:
            break;
        }
        LPG_TRY(generate_sequence(state, generated.cases[i].action, indentation + 1, ecmascript_output));
        if (generated.cases[i].value.is_set)
        {
            LPG_TRY(indent(indentation + 1, ecmascript_output));
            LPG_TRY(generate_register_name(generated.result, ecmascript_output));
            LPG_TRY(stream_writer_write_string(ecmascript_output, " = "));
            LPG_TRY(generate_register_read(state, generated.cases[i].value.value, ecmascript_output));
            LPG_TRY(stream_writer_write_string(ecmascript_output, ";\n"));
        }

        LPG_TRY(indent(indentation, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "}\n"));
    }
    return success_yes;
}

static success_indicator generate_match(function_generation *const state, match_instruction const generated,
                                        const size_t indentation, stream_writer const ecmascript_output)
{
    LPG_TRY(indent(indentation, ecmascript_output));
    LPG_TRY(write_register(state, generated.result, generated.result_type, optional_value_empty, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "undefined;\n"));
    type const key_type = state->registers[generated.key].type_of;
    switch (key_type.kind)
    {
    case type_kind_generic_struct:
        LPG_TO_DO();

    case type_kind_integer_range:
        return generate_equality_comparable_match_cases(state, generated, indentation, ecmascript_output);

    case type_kind_enumeration:
    {
        if (has_stateful_element(state->all_enums[key_type.enum_]))
        {
            return generate_stateful_enum_match_cases(state, generated, indentation, ecmascript_output);
        }
        return generate_equality_comparable_match_cases(state, generated, indentation, ecmascript_output);
    }

    case type_kind_host_value:
    case type_kind_generic_lambda:
    case type_kind_structure:
    case type_kind_function_pointer:
    case type_kind_unit:
    case type_kind_string:
    case type_kind_tuple:
    case type_kind_type:
    case type_kind_enum_constructor:
    case type_kind_lambda:
    case type_kind_interface:
    case type_kind_method_pointer:
    case type_kind_generic_enum:
    case type_kind_generic_interface:
        LPG_UNREACHABLE();
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_lambda_with_captures(function_generation *const state,
                                                       lambda_with_captures_instruction const generated,
                                                       const size_t indentation, stream_writer const ecmascript_output)
{
    LPG_TRY(indent(indentation, ecmascript_output));
    LPG_TRY(write_register(state, generated.into, type_from_lambda(lambda_type_create(generated.lambda)),
                           optional_value_empty, ecmascript_output));
    LPG_TRY(generate_lambda_value_from_registers(
        state, generated.lambda, generated.captures, generated.capture_count, indentation, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, ";\n"));
    return success_yes;
}

static success_indicator generate_get_method(function_generation *const state, get_method_instruction const generated,
                                             const size_t indentation, stream_writer const ecmascript_output)
{
    LPG_TRY(indent(indentation, ecmascript_output));
    LPG_TRY(write_register(state, generated.into,
                           type_from_method_pointer(method_pointer_type_create(generated.interface_, generated.method)),
                           optional_value_empty, ecmascript_output));
    LPG_TRY(generate_register_read(state, generated.from, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, ";\n"));
    return success_yes;
}

static success_indicator generate_new_array(function_generation *const state, new_array_instruction const generated,
                                            const size_t indentation, stream_writer const ecmascript_output)
{
    LPG_TRY(indent(indentation, ecmascript_output));
    LPG_TRY(write_register(
        state, generated.into, type_from_interface(generated.result_type), optional_value_empty, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "new new_array();\n"));
    return success_yes;
}

static success_indicator generate_current_function(function_generation *const state,
                                                   current_function_instruction const generated,
                                                   const size_t indentation, stream_writer const ecmascript_output)
{
    LPG_TRY(write_register(state, generated.into, type_from_function_pointer(state->current_function->signature),
                           optional_value_empty, ecmascript_output));
    LPG_TRY(indent(indentation, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "current_function;\n"));
    return success_yes;
}

static success_indicator generate_erase_type(function_generation *const state, erase_type_instruction const generated,
                                             const size_t indentation, stream_writer const ecmascript_output)
{
    LPG_TRY(indent(indentation, ecmascript_output));
    LPG_TRY(write_register(
        state, generated.into, type_from_interface(generated.impl.target), optional_value_empty, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "new "));
    LPG_TRY(
        generate_implementation_name(generated.impl.target, generated.impl.implementation_index, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "("));
    LPG_TRY(generate_register_read(state, generated.self, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, ");\n"));
    return success_yes;
}

static success_indicator generate_return(function_generation *const state, return_instruction const generated,
                                         size_t const indentation, stream_writer const ecmascript_output)
{
    LPG_TRY(indent(indentation, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "return "));
    LPG_TRY(generate_register_read(state, generated.returned_value, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, ";\n"));
    return success_yes;
}

static success_indicator generate_instruction(function_generation *const state, instruction const generated,
                                              const size_t indentation, stream_writer const ecmascript_output)
{
    switch (generated.type)
    {
    case instruction_current_function:
        return generate_current_function(state, generated.current_function, indentation, ecmascript_output);

    case instruction_new_array:
        return generate_new_array(state, generated.new_array, indentation, ecmascript_output);

    case instruction_get_method:
        return generate_get_method(state, generated.get_method, indentation, ecmascript_output);

    case instruction_call:
        return generate_call(state, generated.call, indentation, ecmascript_output);

    case instruction_return:
        return generate_return(state, generated.return_, indentation, ecmascript_output);

    case instruction_loop:
        return generate_loop(state, generated.loop, indentation, ecmascript_output);

    case instruction_global:
        ASSUME(state->registers[generated.global_into].kind == register_type_none);
        state->registers[generated.global_into] =
            register_info_create(register_type_global, type_from_struct(0), optional_value_empty);
        return success_yes;

    case instruction_read_struct:
        return generate_read_struct(state, generated.read_struct, indentation, ecmascript_output);

    case instruction_break:
        LPG_TRY(indent(indentation, ecmascript_output));
        LPG_TRY(write_register(state, generated.break_into, type_from_unit(), optional_value_create(value_from_unit()),
                               ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "undefined;\n"));
        LPG_TRY(indent(indentation, ecmascript_output));
        return stream_writer_write_string(ecmascript_output, "break;\n");

    case instruction_literal:
        return generate_literal(state, generated.literal, state->all_functions, state->function_count,
                                state->all_interfaces, state->all_structs, indentation, state->all_enums,
                                ecmascript_output);

    case instruction_tuple:
        return generate_tuple(state, generated.tuple_, indentation, ecmascript_output);

    case instruction_instantiate_struct:
        return generate_instantiate_struct(state, generated.instantiate_struct, indentation, ecmascript_output);

    case instruction_enum_construct:
        return generate_enum_construct(state, generated.enum_construct, indentation, ecmascript_output);

    case instruction_match:
        return generate_match(state, generated.match, indentation, ecmascript_output);

    case instruction_get_captures:
        ASSUME(state->registers[generated.global_into].kind == register_type_none);
        state->registers[generated.global_into] = register_info_create(
            register_type_captures, type_from_tuple_type(state->current_function->signature->captures),
            optional_value_empty);
        return success_yes;

    case instruction_lambda_with_captures:
        return generate_lambda_with_captures(state, generated.lambda_with_captures, indentation, ecmascript_output);

    case instruction_erase_type:
        return generate_erase_type(state, generated.erase_type, indentation, ecmascript_output);
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_sequence(function_generation *const state, instruction_sequence const body,
                                           const size_t indentation, stream_writer const ecmascript_output)
{
    for (size_t i = 0; i < body.length; ++i)
    {
        LPG_TRY(generate_instruction(state, body.elements[i], indentation, ecmascript_output));
    }
    return success_yes;
}

static success_indicator generate_function_body(checked_function const function,
                                                checked_function const *const all_functions,
                                                function_id const function_count,
                                                lpg_interface const *const all_interfaces, enumeration const *all_enums,
                                                structure const *all_structs, const size_t indentation,
                                                stream_writer const ecmascript_output)
{
    function_generation state =
        function_generation_create(allocate_array(function.number_of_registers, sizeof(*state.registers)),
                                   all_functions, function_count, all_interfaces, all_enums, all_structs, &function);
    for (register_id i = 0; i < function.number_of_registers; ++i)
    {
        state.registers[i] = register_info_create(register_type_none, type_from_unit(), optional_value_empty);
    }
    ASSUME(function.number_of_registers >= (function.signature->self.is_set + function.signature->parameters.length));
    if (function.signature->self.is_set)
    {
        state.registers[0] =
            register_info_create(register_type_variable, function.signature->self.value, optional_value_empty);
    }
    for (register_id i = 0; i < function.signature->parameters.length; ++i)
    {
        state.registers[function.signature->self.is_set + i] = register_info_create(
            register_type_variable, function.signature->parameters.elements[i], optional_value_empty);
    }
    success_indicator const result = generate_sequence(&state, function.body, indentation + 1, ecmascript_output);
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
    LPG_TRY(stream_writer_write_string(ecmascript_output, "    var current_function = "));
    LPG_TRY(generate_function_name(id, function_count, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, ";\n"));
    LPG_TRY(generate_function_body(
        function, all_functions, function_count, all_interfaces, all_enums, all_structs, 0, ecmascript_output));
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
    char const *const preparation = //
        "(function ()\n"
        "{\n"
        "    \"use strict\";\n"
        "    var string_equals = function (left, right) { return (left === right); };\n"
        "    var integer_equals = function (left, right) { return (left === right); };\n"
        "    var integer_less = function (left, right) { return (left < right); };\n"
        "    var integer_subtract = function (left, right) { var difference = (left - right); return "
        "(difference < 0) ? 1.0 : [0.0, difference]; };\n"
        "    var integer_add = function (left, right) { return [0, (left + right)]; "
        "};\n"
        "    var integer_add_u32 = function (left, right) { var sum = (left + right); return (sum <= 0xffffffff) ? [0, "
        "sum] : 1; "
        "};\n"
        "    var concat = function (left, right) { return (left + right); };\n"
        "    var not = function (argument) { return !argument; };\n"
        "    var side_effect = function () {};\n"
        "    var integer_to_string = function (input) { return \"\" + input; };\n"
        "    var new_array = function (initial_content) {\n"
        "        this.content = initial_content || [];\n"
        "    };\n"
        /* size()*/
        "    new_array.prototype.call_method_0 = function () {\n"
        "        return this.content.length;\n"
        "    };\n"
        /* load()*/
        "    new_array.prototype.call_method_1 = function (index) {\n"
        "        if (index < this.content.length) {\n"
        "            return [0, this.content[index]];\n"
        "        }\n"
        "        return 1;\n"
        "    };\n"
        /* store()*/
        "    new_array.prototype.call_method_2 = function (index, element) {\n"
        "        if (index >= this.content.length) {\n"
        "            return false;\n"
        "        }\n"
        "        this.content[index] = element;\n"
        "        return true;\n"
        "    };\n"
        /* append()*/
        "    new_array.prototype.call_method_3 = function (element) {\n"
        "        this.content.push(element);\n"
        "        return true;\n"
        "    };\n"
        /* clear()*/
        "    new_array.prototype.call_method_4 = function () {\n"
        "        this.content.length = 0;\n"
        "    };\n"
        /* pop()*/
        "    new_array.prototype.call_method_5 = function (count) {\n"
        "        if (count > this.content.length) { return false; }\n"
        "        this.content.length -= count;\n"
        "        return true;\n"
        "    };\n";
    LPG_TRY(stream_writer_write_string(ecmascript_output, preparation));
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
                                   program.enums, program.structs, 0, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "})();\n"));
    return success_yes;
}

success_indicator generate_host_class(stream_writer const destination)
{
    LPG_TRY(stream_writer_write_string(destination, "\
var Host = function ()\n\
{\n\
};\n\
Host.prototype.call_method_0 = function (from, name)\n\
{\n\
   return [0, from[name]];\n\
};\n\
Host.prototype.call_method_1 = function (this_, method, arguments_)\n\
{\n\
   var convertedArguments = [];\n\
   for (var i = 0, c = arguments_.call_method_0(); i < c; ++i)\n\
   {\n\
       convertedArguments.push(arguments_.call_method_1(i)[1]);\n\
   }\n\
   return this_[method].apply(this_, convertedArguments);\n\
};\n\
Host.prototype.call_method_2 = function (content)\n\
{\n\
   return content;\n\
};\n\
Host.prototype.call_method_3 = function (from)\n\
{\n\
   return ((typeof from) === \"string\") ? [0, from] : 1;\n\
};\n\
Host.prototype.call_method_4 = function ()\n\
{\n\
   return undefined;\n\
};\n\
Host.prototype.call_method_5 = function (value)\n\
{\n\
   return value;\n\
};\n\
Host.prototype.call_method_6 = function (left, right)\n\
{\n\
   return (left === right);\n\
};\n\
Host.prototype.call_method_7 = function (from)\n\
{\n\
   return ((typeof from === 'number') && \
       isFinite(from) && \
       (Math.floor(from) === from)) ? [0, from] : 1;\n\
};\n\
"));
    return success_yes;
}
