#include "lpg_javascript_backend.h"
#include "lpg_assert.h"
#include "lpg_instruction.h"
#include "lpg_allocate.h"

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
                                        interface const *const all_interfaces, stream_writer const javascript_output);

static success_indicator generate_enum_element(enum_element_value const element, interface const *const all_interfaces,
                                               stream_writer const javascript_output)
{
    if (element.state)
    {
        LPG_TRY(stream_writer_write_string(javascript_output, "["));
        LPG_TRY(stream_writer_write_integer(javascript_output, integer_create(0, element.which)));
        LPG_TRY(stream_writer_write_string(javascript_output, ", "));
        LPG_TRY(generate_value(*element.state, element.state_type, all_interfaces, javascript_output));
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

static success_indicator generate_value(value const generated, type const type_of,
                                        interface const *const all_interfaces, stream_writer const javascript_output)
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
                               all_interfaces, javascript_output));
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
        return generate_function_name(generated.function_pointer.code, javascript_output);

    case value_kind_flat_object:
        LPG_TO_DO();

    case value_kind_type:
        return stream_writer_write_string(javascript_output, "/*TODO type*/ undefined");

    case value_kind_enum_element:
        return generate_enum_element(generated.enum_element, all_interfaces, javascript_output);

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
            LPG_TRY(generate_value(
                generated.tuple_.elements[i], type_of.tuple_.elements[i], all_interfaces, javascript_output));
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

typedef struct function_generation
{
    register_type *registers;
    checked_function const *all_functions;
    interface const *all_interfaces;
} function_generation;

static function_generation function_generation_create(register_type *registers, checked_function const *all_functions,
                                                      interface const *all_interfaces)
{
    function_generation const result = {registers, all_functions, all_interfaces};
    return result;
}

static success_indicator write_register(function_generation *const state, register_id const which,
                                        stream_writer const javascript_output)
{
    ASSUME(state->registers[which] == register_type_none);
    state->registers[which] = register_type_variable;
    LPG_TRY(generate_var(which, javascript_output));
    return success;
}

static success_indicator generate_literal(function_generation *const state, literal_instruction const generated,
                                          interface const *const all_interfaces, stream_writer const javascript_output)
{
    LPG_TRY(write_register(state, generated.into, javascript_output));
    LPG_TRY(generate_value(generated.value_, generated.type_of, all_interfaces, javascript_output));
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
    switch (state->registers[generated.from_object])
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
    LPG_TRY(write_register(state, generated.into, javascript_output));
    LPG_TRY(generate_read_struct_value(state, generated, javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, ";\n"));
    return success;
}

static success_indicator generate_call(function_generation *const state, call_instruction const generated,
                                       stream_writer const javascript_output)
{
    LPG_TRY(write_register(state, generated.result, javascript_output));
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
    LPG_TRY(write_register(state, construct.into, javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, "["));
    LPG_TRY(stream_writer_write_integer(javascript_output, integer_create(0, construct.which)));
    LPG_TRY(stream_writer_write_string(javascript_output, ", "));
    LPG_TRY(generate_register_name(construct.state, javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, "];\n"));
    return success;
}

static success_indicator generate_tuple(function_generation *const state, tuple_instruction const generated,
                                        stream_writer const javascript_output)
{
    LPG_TRY(write_register(state, generated.result, javascript_output));
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

static success_indicator generate_match(function_generation *const state, match_instruction const generated,
                                        stream_writer const javascript_output)
{
    LPG_TRY(write_register(state, generated.result, javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, "undefined;\n"));
    for (size_t i = 0; i < generated.count; ++i)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(javascript_output, "else "));
        }
        LPG_TRY(stream_writer_write_string(javascript_output, "if ("));
        LPG_TRY(generate_register_name(generated.key, javascript_output));
        LPG_TRY(stream_writer_write_string(javascript_output, " === "));
        LPG_TRY(generate_register_name(generated.cases[i].key, javascript_output));
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

static success_indicator generate_lambda_with_captures(function_generation *const state,
                                                       lambda_with_captures_instruction const generated,
                                                       stream_writer const javascript_output)
{
    LPG_TRY(write_register(state, generated.into, javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, "function ("));
    checked_function const *const function = &state->all_functions[generated.lambda];
    for (register_id i = 0; i < function->signature->parameters.length; ++i)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(javascript_output, ", "));
        }
        LPG_TRY(generate_register_name(i, javascript_output));
    }
    LPG_TRY(stream_writer_write_string(javascript_output, ") { return "));
    LPG_TRY(generate_function_name(generated.lambda, javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, "("));
    bool comma = false;
    for (register_id i = 0; i < generated.capture_count; ++i)
    {
        if (comma)
        {
            LPG_TRY(stream_writer_write_string(javascript_output, ", "));
        }
        else
        {
            comma = true;
        }
        LPG_TRY(generate_register_name(generated.captures[i], javascript_output));
    }
    for (register_id i = 0; i < function->signature->parameters.length; ++i)
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
    LPG_TRY(stream_writer_write_string(javascript_output, "); };\n"));
    return success;
}

static success_indicator generate_get_method(function_generation *const state, get_method_instruction const generated,
                                             stream_writer const javascript_output)
{
    LPG_TRY(write_register(state, generated.into, javascript_output));
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
    LPG_TRY(write_register(state, generated.into, javascript_output));
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
        ASSUME(state->registers[generated.global_into] == register_type_none);
        state->registers[generated.global_into] = register_type_global;
        return success;

    case instruction_read_struct:
        return generate_read_struct(state, generated.read_struct, javascript_output);

    case instruction_break:
        LPG_TRY(write_register(state, generated.break_into, javascript_output));
        LPG_TRY(stream_writer_write_string(javascript_output, "undefined;\n"));
        return stream_writer_write_string(javascript_output, "break;\n");

    case instruction_literal:
        return generate_literal(state, generated.literal, state->all_interfaces, javascript_output);

    case instruction_tuple:
        return generate_tuple(state, generated.tuple_, javascript_output);

    case instruction_enum_construct:
        return generate_enum_construct(state, generated.enum_construct, javascript_output);

    case instruction_match:
        return generate_match(state, generated.match, javascript_output);

    case instruction_get_captures:
        ASSUME(state->registers[generated.global_into] == register_type_none);
        state->registers[generated.global_into] = register_type_captures;
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
                                                interface const *const all_interfaces,
                                                stream_writer const javascript_output)
{
    function_generation state = function_generation_create(
        allocate_array(function.number_of_registers, sizeof(*state.registers)), all_functions, all_interfaces);
    for (register_id i = 0; i < function.number_of_registers; ++i)
    {
        state.registers[i] = register_type_none;
    }
    for (register_id i = 0; i < function.signature->parameters.length; ++i)
    {
        state.registers[i] = register_type_variable;
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
                                         interface const *const all_interfaces, stream_writer const javascript_output)
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
    LPG_TRY(generate_function_body(function, all_functions, all_interfaces, javascript_output));
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
        LPG_TRY(define_function(i, program.functions[i], program.functions, program.interfaces, javascript_output));
    }
    ASSUME(program.functions[0].signature->parameters.length == 0);
    LPG_TRY(generate_function_body(program.functions[0], program.functions, program.interfaces, javascript_output));
    LPG_TRY(stream_writer_write_string(javascript_output, "})();\n"));
    return success;
}
