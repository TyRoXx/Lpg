#include "lpg_ecmascript_backend.h"
#include "lpg_allocate.h"
#include "lpg_assert.h"
#include "lpg_enum_encoding.h"
#include "lpg_function_generation.h"
#include "lpg_instruction.h"
#include "lpg_standard_library.h"
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

static success_indicator escape_identifier(unicode_view const original, stream_writer const ecmascript_output)
{
    for (size_t i = 0; i < original.length; ++i)
    {
        char const c = original.begin[i];
        switch (c)
        {
        case '-':
            LPG_TRY(stream_writer_write_string(ecmascript_output, "_"));
            break;

        case '_':
            LPG_TRY(stream_writer_write_string(ecmascript_output, "__"));
            break;

        default:
            LPG_TRY(stream_writer_write_unicode_view(ecmascript_output, unicode_view_create(&c, 1)));
            break;
        }
    }
    return success_yes;
}

static success_indicator generate_register_name(checked_function const *const function, register_id const id,
                                                stream_writer const ecmascript_output)
{
    ASSUME(function);
    LPG_TRY(stream_writer_write_string(ecmascript_output, "r"));
    LPG_TRY(stream_writer_write_integer(ecmascript_output, integer_create(0, id)));
    ASSUME(id < function->number_of_registers);
    unicode_string const *const name = function->register_debug_names + id;
    if (name->length > 0)
    {
        LPG_TRY(stream_writer_write_string(ecmascript_output, "_"));
        LPG_TRY(escape_identifier(unicode_view_from_string(*name), ecmascript_output));
    }
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
            break;
        }
    }
    LPG_TRY(stream_writer_write_string(ecmascript_output, "\""));
    return success_yes;
}

static success_indicator generate_enum_constructor(enum_encoding_strategy_cache *const strategy_cache,
                                                   enum_constructor_type const constructor,
                                                   stream_writer const ecmascript_output)
{
    enum_encoding_strategy *const strategy =
        enum_encoding_strategy_cache_require(strategy_cache, constructor.enumeration);
    enum_encoding_element const element = strategy->elements[constructor.which];
    ASSUME(element.has_state);
    LPG_TRY(stream_writer_write_string(ecmascript_output, "function (state) { return "));
    LPG_TRY(enum_construct_stateful_begin(element.stateful, constructor.which, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "state"));
    LPG_TRY(enum_construct_stateful_end(element.stateful, ecmascript_output));
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

static success_indicator generate_lambda_value_from_values(
    checked_function const *const current_function, enum_encoding_strategy_cache *const strategy_cache,
    checked_function const *all_functions, function_id const function_count, lpg_interface const *const all_interfaces,
    structure const *const all_structs, function_id const lambda, value const *const captures,
    size_t const capture_count, enumeration const *const all_enums, stream_writer const ecmascript_output)
{
    LPG_TRY(stream_writer_write_string(ecmascript_output, "function ("));
    checked_function const *const function = &all_functions[lambda];
    for (register_id i = 0; i < function->signature->parameters.length; ++i)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
        }
        LPG_TRY(generate_register_name(function, i, ecmascript_output));
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
        LPG_TRY(generate_value(current_function, strategy_cache, captures[i], function->signature->captures.elements[i],
                               all_functions, function_count, all_interfaces, all_structs, all_enums,
                               ecmascript_output));
    }
    ASSUME(comma);
    for (register_id i = 0; i < function->signature->parameters.length; ++i)
    {
        LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
        LPG_TRY(generate_register_name(function, i, ecmascript_output));
    }
    LPG_TRY(stream_writer_write_string(ecmascript_output, "); }"));
    return success_yes;
}

static success_indicator generate_array_value(checked_function const *const current_function,
                                              enum_encoding_strategy_cache *const strategy_cache,
                                              array_value const generated, checked_function const *all_functions,
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
        LPG_TRY(generate_value(current_function, strategy_cache, generated.elements[i], generated.element_type,
                               all_functions, function_count, all_interfaces, all_structs, all_enums,
                               ecmascript_output));
    }
    LPG_TRY(stream_writer_write_string(ecmascript_output, "])"));
    return success_yes;
}

success_indicator generate_value(checked_function const *const current_function,
                                 enum_encoding_strategy_cache *const strategy_cache, value const generated,
                                 type const type_of, checked_function const *all_functions,
                                 function_id const function_count, lpg_interface const *const all_interfaces,
                                 structure const *const all_structs, enumeration const *const all_enums,
                                 stream_writer const ecmascript_output)
{
    ASSUME(value_conforms_to_type(generated, type_of));
    ASSUME(value_is_valid(generated));
    switch (generated.kind)
    {
    case value_kind_array:
        return generate_array_value(current_function, strategy_cache, *generated.array, all_functions, function_count,
                                    all_interfaces, all_structs, all_enums, ecmascript_output);

    case value_kind_pattern:
        LPG_TO_DO();

    case value_kind_type_erased:
        LPG_TRY(stream_writer_write_string(ecmascript_output, "new "));
        LPG_TRY(generate_implementation_name(
            generated.type_erased.impl.target, generated.type_erased.impl.implementation_index, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "("));
        LPG_TRY(generate_value(current_function, strategy_cache, *generated.type_erased.self,
                               all_interfaces[generated.type_erased.impl.target]
                                   .implementations[generated.type_erased.impl.implementation_index]
                                   .self,
                               all_functions, function_count, all_interfaces, all_structs, all_enums,
                               ecmascript_output));
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
                current_function, strategy_cache, all_functions, function_count, all_interfaces, all_structs,
                generated.function_pointer.code, generated.function_pointer.captures,
                generated.function_pointer.capture_count, all_enums, ecmascript_output);
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
        return generate_enum_element(current_function, strategy_cache, generated.enum_element,
                                     all_enums + type_of.enum_, all_functions, function_count, all_interfaces,
                                     all_structs, all_enums, ecmascript_output);

    case value_kind_unit:
        return stream_writer_write_string(ecmascript_output, "/*unit*/ undefined");

    case value_kind_generic_lambda:
        return stream_writer_write_string(ecmascript_output, "/*generic lambda*/ undefined");

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
            LPG_TRY(generate_value(current_function, strategy_cache, generated.structure.members[i],
                                   object.members[i].what, all_functions, function_count, all_interfaces, all_structs,
                                   all_enums, ecmascript_output));
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
            LPG_TRY(generate_value(current_function, strategy_cache, generated.tuple_.elements[i],
                                   type_of.tuple_.elements[i], all_functions, function_count, all_interfaces,
                                   all_structs, all_enums, ecmascript_output));
        }
        LPG_TRY(stream_writer_write_string(ecmascript_output, "]"));
        return success_yes;
    }

    case value_kind_enum_constructor:
        return generate_enum_constructor(strategy_cache, *type_of.enum_constructor, ecmascript_output);
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_var(checked_function const *const current_function, register_id const id,
                                      stream_writer const ecmascript_output)
{
    LPG_TRY(stream_writer_write_string(ecmascript_output, "var "));
    LPG_TRY(generate_register_name(current_function, id, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, " = "));
    return success_yes;
}

static success_indicator write_register(function_generation *const state, register_id const which, type const type_of,
                                        optional_value const known_value, stream_writer const ecmascript_output)
{
    ASSUME(which < state->current_function->number_of_registers);
    ASSUME(state->registers[which].kind == register_type_none);
    size_t const declaration_begin = *state->current_output_position;
    LPG_TRY(generate_var(state->current_function, which, ecmascript_output));
    size_t const declaration_end = *state->current_output_position;
    state->registers[which] =
        register_info_create(register_type_variable, type_of, known_value, declaration_begin, declaration_end, false);
    return success_yes;
}

static void mark_register_as_used(function_generation *const state, register_id const id)
{
    state->registers[id].is_ever_used = true;
}

success_indicator generate_register_read(function_generation *const state, register_id const id,
                                         stream_writer const ecmascript_output)
{
    register_info const info = state->registers[id];
    if (!info.known_value.is_set)
    {
        mark_register_as_used(state, id);
        return generate_register_name(state->current_function, id, ecmascript_output);
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
    case value_kind_enum_element:
        return generate_value(state->current_function, state->strategy_cache, info.known_value.value_, info.type_of,
                              state->all_functions, state->function_count, state->all_interfaces, state->all_structs,
                              state->all_enums, ecmascript_output);

    case value_kind_function_pointer:
        if (info.known_value.value_.function_pointer.capture_count > 0)
        {
            mark_register_as_used(state, id);
            return generate_register_name(state->current_function, id, ecmascript_output);
        }
        return generate_value(state->current_function, state->strategy_cache, info.known_value.value_, info.type_of,
                              state->all_functions, state->function_count, state->all_interfaces, state->all_structs,
                              state->all_enums, ecmascript_output);

    case value_kind_structure:
    case value_kind_tuple:
    case value_kind_enum_constructor:
    case value_kind_type_erased:
    case value_kind_array:
        mark_register_as_used(state, id);
        return generate_register_name(state->current_function, id, ecmascript_output);
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
        LPG_TRY(generate_register_name(state->current_function, i, ecmascript_output));
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
        LPG_TRY(generate_register_name(state->current_function, i, ecmascript_output));
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
    LPG_TRY(generate_value(state->current_function, state->strategy_cache, generated.value_, generated.type_of,
                           all_functions, function_count, all_interfaces, all_structs, all_enums, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, ";"));
    state->registers[generated.into].declaration_end = *state->current_output_position;
    LPG_TRY(stream_writer_write_string(ecmascript_output, "\n"));
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
            return stream_writer_write_string(ecmascript_output, "/*boolean*/ undefined");

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
    LPG_TRY(stream_writer_write_string(ecmascript_output, ";"));
    state->registers[generated.into].declaration_end = *state->current_output_position;
    LPG_TRY(stream_writer_write_string(ecmascript_output, "\n"));
    return success_yes;
}

static success_indicator generate_method_name(size_t const method_index, lpg_interface const *const interface_,
                                              stream_writer const ecmascript_output)
{
    ASSUME(interface_);
    LPG_TRY(stream_writer_write_string(ecmascript_output, "m"));
    LPG_TRY(stream_writer_write_integer(ecmascript_output, integer_create(0, method_index)));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "_"));
    LPG_TRY(escape_identifier(unicode_view_from_string(interface_->methods[method_index].name), ecmascript_output));
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
        LPG_TRY(stream_writer_write_string(ecmascript_output, "."));
        LPG_TRY(generate_method_name(callee_type.method_pointer.method_index,
                                     state->all_interfaces + callee_type.method_pointer.interface_, ecmascript_output));
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
    LPG_TRY(generate_value(state->current_function, state->strategy_cache, value_from_unit(), type_from_unit(),
                           state->all_functions, state->function_count, state->all_interfaces, state->all_structs,
                           state->all_enums, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, ";"));
    state->registers[generated.unit_goes_into].declaration_end = *state->current_output_position;
    LPG_TRY(stream_writer_write_string(ecmascript_output, "\n"));
    return success_yes;
}

static success_indicator generate_enum_construct(function_generation *const state,
                                                 enum_construct_instruction const construct, const size_t indentation,
                                                 stream_writer const ecmascript_output)
{
    enum_encoding_strategy *const strategy =
        enum_encoding_strategy_cache_require(state->strategy_cache, construct.which.enumeration);
    enum_encoding_element const element = strategy->elements[construct.which.which];
    ASSUME(element.has_state);
    LPG_TRY(indent(indentation, ecmascript_output));
    LPG_TRY(write_register(state, construct.into, type_from_enumeration(construct.which.enumeration),
                           optional_value_empty, ecmascript_output));
    LPG_TRY(enum_construct_stateful_begin(element.stateful, construct.which.which, ecmascript_output));
    LPG_TRY(generate_register_read(state, construct.state, ecmascript_output));
    LPG_TRY(enum_construct_stateful_end(element.stateful, ecmascript_output));
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
            mark_register_as_used(state, generated.result);
            LPG_TRY(generate_register_name(state->current_function, generated.result, ecmascript_output));
            LPG_TRY(stream_writer_write_string(ecmascript_output, " = "));
            LPG_TRY(generate_register_read(state, generated.cases[i].value.value, ecmascript_output));
            LPG_TRY(stream_writer_write_string(ecmascript_output, ";\n"));
        }
        LPG_TRY(indent(indentation, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "}\n"));
    }
    LPG_TRY(indent(indentation, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "else { fail(); }\n"));
    return success_yes;
}

static success_indicator generate_stateful_enum_match_cases(function_generation *const state,
                                                            match_instruction const generated, enum_id const enum_,
                                                            const size_t indentation,
                                                            stream_writer const ecmascript_output)
{
    enum_encoding_strategy const *const strategy = enum_encoding_strategy_cache_require(state->strategy_cache, enum_);
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
            enum_element_id which = generated.cases[i].stateful_enum.element;
            enum_encoding_element const element_strategy = strategy->elements[which];
            ASSUME(element_strategy.has_state);
            LPG_TRY(
                stateful_enum_case_check(state, generated.key, element_strategy.stateful, which, ecmascript_output));
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
            enum_id const enumeration_id = state->registers[generated.key].type_of.enum_;
            enum_element_id const which = generated.cases[i].stateful_enum.element;
            optional_type const enum_state = state->all_enums[enumeration_id].elements[which].state;
            ASSUME(enum_state.is_set);
            LPG_TRY(write_register(state, generated.cases[i].stateful_enum.where, enum_state.value,
                                   optional_value_empty, ecmascript_output));
            LPG_TRY(generate_register_read(state, generated.key, ecmascript_output));
            LPG_TRY(stateful_enum_get_state(state->strategy_cache, enumeration_id, which, ecmascript_output));
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
            mark_register_as_used(state, generated.result);
            LPG_TRY(generate_register_name(state->current_function, generated.result, ecmascript_output));
            LPG_TRY(stream_writer_write_string(ecmascript_output, " = "));
            LPG_TRY(generate_register_read(state, generated.cases[i].value.value, ecmascript_output));
            LPG_TRY(stream_writer_write_string(ecmascript_output, ";\n"));
        }

        LPG_TRY(indent(indentation, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "}\n"));
    }
    LPG_TRY(indent(indentation, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "else { fail(); }\n"));
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
            return generate_stateful_enum_match_cases(state, generated, key_type.enum_, indentation, ecmascript_output);
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
    lpg_interface const *const array = state->all_interfaces + generated.result_type;
    method_description const load = array->methods[1];
    ASSUME(load.result.kind == type_kind_enumeration);
    LPG_TRY(stream_writer_write_string(ecmascript_output, "new new_array([], "));
    LPG_TRY(generate_enum_constructor(
        state->strategy_cache, enum_constructor_type_create(load.result.enum_, 0), ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
    LPG_TRY(generate_enum_element(state->current_function, state->strategy_cache,
                                  enum_element_value_create(1, NULL, type_from_unit()),
                                  state->all_enums + load.result.enum_, state->all_functions, state->function_count,
                                  state->all_interfaces, state->all_structs, state->all_enums, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, ");"));
    state->registers[generated.into].declaration_end = *state->current_output_position;
    LPG_TRY(stream_writer_write_string(ecmascript_output, "\n"));
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
        state->registers[generated.global_into] = register_info_create(
            register_type_global, type_from_struct(0), optional_value_empty, ~(size_t)0, ~(size_t)0, false);
        return success_yes;

    case instruction_read_struct:
        return generate_read_struct(state, generated.read_struct, indentation, ecmascript_output);

    case instruction_break:
        LPG_TRY(indent(indentation, ecmascript_output));
        LPG_TRY(write_register(state, generated.break_into, type_from_unit(), optional_value_create(value_from_unit()),
                               ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "undefined;"));
        state->registers[generated.break_into].declaration_end = *state->current_output_position;
        LPG_TRY(stream_writer_write_string(ecmascript_output, "\n"));
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
            optional_value_empty, ~(size_t)0, ~(size_t)0, false);
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

static success_indicator
generate_function_body(enum_encoding_strategy_cache *strategy_cache, checked_function const function,
                       checked_function const *const all_functions, function_id const function_count,
                       lpg_interface const *const all_interfaces, enumeration const *all_enums,
                       structure const *all_structs, const size_t indentation, memory_writer *const ecmascript_output,
                       size_t *const current_output_position)
{
    function_generation state = function_generation_create(
        allocate_array(function.number_of_registers, sizeof(*state.registers)), all_functions, function_count,
        all_interfaces, all_enums, all_structs, &function, strategy_cache, current_output_position);
    for (register_id i = 0; i < function.number_of_registers; ++i)
    {
        state.registers[i] = register_info_create(
            register_type_none, type_from_unit(), optional_value_empty, ~(size_t)0, ~(size_t)0, false);
    }
    ASSUME(function.number_of_registers >= (function.signature->self.is_set + function.signature->parameters.length));
    if (function.signature->self.is_set)
    {
        state.registers[0] = register_info_create(
            register_type_variable, function.signature->self.value, optional_value_empty, ~(size_t)0, ~(size_t)0, true);
    }
    for (register_id i = 0; i < function.signature->parameters.length; ++i)
    {
        state.registers[function.signature->self.is_set + i] =
            register_info_create(register_type_variable, function.signature->parameters.elements[i],
                                 optional_value_empty, ~(size_t)0, ~(size_t)0, true);
    }
    success_indicator const result =
        generate_sequence(&state, function.body, indentation + 1, memory_writer_erase(ecmascript_output));
    for (register_id i = 0; i < function.number_of_registers; ++i)
    {
        register_info *const register_ = state.registers + i;
        if (register_->is_ever_used)
        {
            continue;
        }
        switch (register_->kind)
        {
        case register_type_captures:
        case register_type_global:
        case register_type_none:
            break;

        case register_type_variable:
        {
            // we will later remove all zero characters from the generated source
            memset((ecmascript_output->data + register_->declaration_begin), 0,
                   (register_->declaration_end - register_->declaration_begin));
            break;
        }
        }
    }
    deallocate(state.registers);
    return result;
}

static success_indicator generate_argument_list(checked_function const *const current_function, size_t const length,
                                                stream_writer const ecmascript_output)
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
        LPG_TRY(generate_register_name(current_function, i, ecmascript_output));
    }
    return success_yes;
}

static success_indicator generate_artificial_argument_list(size_t const length, stream_writer const ecmascript_output)
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
        LPG_TRY(stream_writer_write_string(ecmascript_output, "arg"));
        LPG_TRY(stream_writer_write_integer(ecmascript_output, integer_create(0, i)));
    }
    return success_yes;
}

static success_indicator define_function(enum_encoding_strategy_cache *strategy_cache, function_id const id,
                                         checked_function const function, checked_function const *const all_functions,
                                         function_id const function_count, lpg_interface const *const all_interfaces,
                                         enumeration const *all_enums, structure const *all_structs,
                                         memory_writer *const ecmascript_output, size_t *const current_output_position)
{
    stream_writer const writer = memory_writer_erase(ecmascript_output);
    LPG_TRY(generate_function_name(id, function_count, writer));
    LPG_TRY(stream_writer_write_string(writer, " = function ("));
    bool comma = false;
    for (register_id i = 0; i < function.signature->captures.length; ++i)
    {
        if (comma)
        {
            LPG_TRY(stream_writer_write_string(writer, ", "));
        }
        else
        {
            comma = true;
        }
        LPG_TRY(generate_capture_name(i, writer));
    }
    size_t const total_parameters = (function.signature->parameters.length + function.signature->self.is_set);
    if (total_parameters > 0)
    {
        if (comma)
        {
            LPG_TRY(stream_writer_write_string(writer, ", "));
        }
        LPG_TRY(generate_argument_list(&function, total_parameters, writer));
    }
    LPG_TRY(stream_writer_write_string(writer, ")\n"));
    LPG_TRY(stream_writer_write_string(writer, "{\n"));
    LPG_TRY(stream_writer_write_string(writer, "    var current_function = "));
    LPG_TRY(generate_function_name(id, function_count, writer));
    LPG_TRY(stream_writer_write_string(writer, ";\n"));
    LPG_TRY(generate_function_body(strategy_cache, function, all_functions, function_count, all_interfaces, all_enums,
                                   all_structs, 0, ecmascript_output, current_output_position));
    LPG_TRY(stream_writer_write_string(writer, "};\n"));
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
        LPG_TRY(stream_writer_write_string(ecmascript_output, ".prototype."));
        LPG_TRY(generate_method_name(i, implemented_interface, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, " = function ("));
        LPG_TRY(generate_artificial_argument_list(parameter_count, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, ") {\n"));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "    return "));
        LPG_TRY(generate_function_name(current_method.code, function_count, ecmascript_output));
        LPG_TRY(stream_writer_write_string(ecmascript_output, "(this.self"));
        if (parameter_count > 0)
        {
            LPG_TRY(stream_writer_write_string(ecmascript_output, ", "));
        }
        LPG_TRY(generate_artificial_argument_list(parameter_count, ecmascript_output));
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

static unicode_string wrap_c_str_in_unicode_string(char const *const c_str)
{
    unicode_string const result = {(char *)c_str, strlen(c_str)};
    return result;
}

lpg_interface make_array_interface(method_description array_methods[6])
{
    array_methods[0] =
        method_description_create(wrap_c_str_in_unicode_string("size"), tuple_type_create(NULL, 0), type_from_unit());
    array_methods[1] =
        method_description_create(wrap_c_str_in_unicode_string("load"), tuple_type_create(NULL, 0), type_from_unit());
    array_methods[2] =
        method_description_create(wrap_c_str_in_unicode_string("store"), tuple_type_create(NULL, 0), type_from_unit());
    array_methods[3] =
        method_description_create(wrap_c_str_in_unicode_string("append"), tuple_type_create(NULL, 0), type_from_unit());
    array_methods[4] =
        method_description_create(wrap_c_str_in_unicode_string("clear"), tuple_type_create(NULL, 0), type_from_unit());
    array_methods[5] =
        method_description_create(wrap_c_str_in_unicode_string("pop"), tuple_type_create(NULL, 0), type_from_unit());
    lpg_interface const array_interface = interface_create(array_methods, 6, NULL, 0);
    return array_interface;
}

static void remove_unnecessary_code(memory_writer *const ecmascript_output)
{
    size_t non_zeroes = 0;
    char *const data = ecmascript_output->data;
    size_t const original_size = ecmascript_output->used;
    for (size_t i = 0; i < original_size;)
    {
        char const c = data[i];
        if (c)
        {
            data[non_zeroes] = c;
            ++non_zeroes;
            ++i;
        }
        else
        {
            ++i;
            while ((i < original_size) && (data[i] == '\0'))
            {
                ++i;
            }
            if (i < original_size)
            {
                // if we remove the whole line's content, we should remove the line feed
                // and the indentation as well
                if (data[i] == '\n')
                {
                    ++i;
                    while ((i < original_size) && (data[i] == ' '))
                    {
                        ++i;
                    }
                }
            }
        }
    }
    ecmascript_output->used = non_zeroes;
}

success_indicator generate_ecmascript(checked_program const program, enum_encoding_strategy_cache *strategy_cache,
                                      memory_writer *const ecmascript_output)
{
    stream_writer const writer = memory_writer_erase(ecmascript_output);
    LPG_TRY(stream_writer_write_string(writer,
                                       "(function ()\n"
                                       "{\n"
                                       "    \"use strict\";\n"
                                       "    var globalObject = new Function('return this;')();\n"
                                       "    var fail = function (condition) { if (globalObject.builtin_fail) { "
                                       "builtin_fail(); } else { throw "
                                       "\"fail\"; } };\n"
                                       "    var assert = function (condition) { if "
                                       "(globalObject.builtin_assert) { builtin_assert(condition); } else "
                                       "if "
                                       "(!condition) { fail(); } };\n"
                                       "    var string_equals = function (left, right) { return (left === "
                                       "right); };\n"
                                       "    var integer_equals = function (left, right) { return (left === "
                                       "right); };\n"
                                       "    var integer_less = function (left, right) { return (left < right); "
                                       "};\n"
                                       "    var integer_subtract = function (left, right) { var difference = "
                                       "(left - right); return "
                                       "(difference < 0) ? undefined : difference; };\n"
                                       "    var integer_add = function (left, right) { return (left + right); "
                                       "};\n"
                                       "    var integer_add_u32 = function (left, right) { var sum = (left + "
                                       "right); return (sum <= 0xffffffff) ? "
                                       "sum : undefined; "
                                       "};\n"
                                       "    var concat = function (left, right) { return (left + right); };\n"
                                       "    var not = function (argument) { return !argument; };\n"
                                       "    var side_effect = function () {};\n"
                                       "    var integer_to_string = function (input) { return \"\" + input; };\n"
                                       "    var new_array = function (initial_content, make_some, none) {\n"
                                       "        this.content = initial_content || [];\n"
                                       "        this.make_some = make_some;\n"
                                       "        this.none = none;\n"
                                       "    };\n"
                                       /* size()*/
                                       "    new_array.prototype."));
    method_description array_methods[6];
    lpg_interface const array_interface = make_array_interface(array_methods);
    LPG_TRY(generate_method_name(0, &array_interface, writer));
    LPG_TRY(stream_writer_write_string(writer, " = function () {\n"
                                               "        return this.content.length;\n"
                                               "    };\n"
                                               /* load()*/
                                               "    new_array.prototype."));
    LPG_TRY(generate_method_name(1, &array_interface, writer));
    LPG_TRY(stream_writer_write_string(writer, " = function (index) {\n"
                                               "        if (index < this.content.length) {\n"
                                               "            return this.make_some(this.content[index]);\n"
                                               "        }\n"
                                               "        return this.none;\n"
                                               "    };\n"
                                               /* store()*/
                                               "    new_array.prototype."));
    LPG_TRY(generate_method_name(2, &array_interface, writer));
    LPG_TRY(stream_writer_write_string(writer, " = function (index, element) {\n"
                                               "        if (index >= this.content.length) {\n"
                                               "            return false;\n"
                                               "        }\n"
                                               "        this.content[index] = element;\n"
                                               "        return true;\n"
                                               "    };\n"
                                               /* append()*/
                                               "    new_array.prototype."));
    LPG_TRY(generate_method_name(3, &array_interface, writer));
    LPG_TRY(stream_writer_write_string(writer, " = function (element) {\n"
                                               "        this.content.push(element);\n"
                                               "        return true;\n"
                                               "    };\n"
                                               /* clear()*/
                                               "    new_array.prototype."));
    LPG_TRY(generate_method_name(4, &array_interface, writer));
    LPG_TRY(stream_writer_write_string(writer, " = function () {\n"
                                               "        this.content.length = 0;\n"
                                               "    };\n"
                                               /* pop()*/
                                               "    new_array.prototype."));
    LPG_TRY(generate_method_name(5, &array_interface, writer));
    LPG_TRY(stream_writer_write_string(writer, " = function (count) {\n"
                                               "        if (count > this.content.length) { return false; }\n"
                                               "        this.content.length -= count;\n"
                                               "        return true;\n"
                                               "    };\n"));
    for (interface_id i = 0; i < program.interface_count; ++i)
    {
        LPG_TRY(define_interface(i, program.interfaces[i], program.function_count, writer));
    }
    for (function_id i = 1; i < program.function_count; ++i)
    {
        LPG_TRY(declare_function(i, program.function_count, writer));
    }
    for (function_id i = 1; i < program.function_count; ++i)
    {
        LPG_TRY(define_function(strategy_cache, i, program.functions[i], program.functions, program.function_count,
                                program.interfaces, program.enums, program.structs, ecmascript_output,
                                &ecmascript_output->used));
    }
    ASSUME(program.functions[0].signature->parameters.length == 0);
    LPG_TRY(generate_function_body(strategy_cache, program.functions[0], program.functions, program.function_count,
                                   program.interfaces, program.enums, program.structs, 0, ecmascript_output,
                                   &ecmascript_output->used));
    LPG_TRY(stream_writer_write_string(writer, "})();\n"));
    remove_unnecessary_code(ecmascript_output);
    return success_yes;
}

success_indicator generate_host_class(enum_encoding_strategy_cache *const strategy_cache,
                                      lpg_interface const *const host, lpg_interface const *const all_interfaces,
                                      stream_writer const ecmascript_output)
{
    ASSUME(host);
    LPG_TRY(stream_writer_write_string(ecmascript_output, "\
var Host = function ()\n\
{\n\
};\n\
Host.prototype."));
    LPG_TRY(generate_method_name(0, host, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, " = function (from, name)\n\
{\n\
   return "));
    LPG_TRY(enum_construct_stateful_begin(enum_encoding_element_stateful_from_indirect(), 0, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "from[name]"));
    LPG_TRY(enum_construct_stateful_end(enum_encoding_element_stateful_from_indirect(), ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, ";\n\
};\n\
Host.prototype."));
    LPG_TRY(generate_method_name(1, host, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, " = function (this_, method, arguments_)\n\
{\n\
   var convertedArguments = [];\n\
   for (var i = 0, c = arguments_."));
    method_description array_methods[6];
    lpg_interface const array_interface = make_array_interface(array_methods);
    LPG_TRY(generate_method_name(0, &array_interface, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "(); i < c; ++i)\n\
   {\n\
       convertedArguments.push(arguments_."));
    LPG_TRY(generate_method_name(1, &array_interface, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "(i)"));
    {
        method_description const call_method = host->methods[1];
        ASSUME(call_method.parameters.length == 3);
        type const arguments = call_method.parameters.elements[2];
        ASSUME(arguments.kind == type_kind_interface);
        lpg_interface const *const array_of_host_value = all_interfaces + arguments.interface_;
        method_description const load = array_of_host_value->methods[1];
        ASSUME(load.parameters.length == 1);
        type const load_result = load.result;
        ASSUME(load_result.kind == type_kind_enumeration);
        LPG_TRY(stateful_enum_get_state(strategy_cache, load_result.enum_, 0, ecmascript_output));
    }
    LPG_TRY(stream_writer_write_string(ecmascript_output, ");\n\
   }\n\
   return this_[method].apply(this_, convertedArguments);\n\
};\n\
Host.prototype."));
    LPG_TRY(generate_method_name(2, host, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, " = function (content)\n\
{\n\
   return content;\n\
};\n\
Host.prototype."));
    LPG_TRY(generate_method_name(3, host, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, " = function (from)\n\
{\n\
   return ((typeof from) === \"string\") ? "));
    LPG_TRY(enum_construct_stateful_begin(
        enum_encoding_element_stateful_from_direct(ecmascript_value_set_create_string()), 0, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "from"));
    LPG_TRY(enum_construct_stateful_end(
        enum_encoding_element_stateful_from_direct(ecmascript_value_set_create_string()), ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, " : "));
    LPG_TRY(generate_stateless_enum_element(1, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, ";\n\
};\n\
Host.prototype."));
    LPG_TRY(generate_method_name(4, host, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, " = function ()\n\
{\n\
   return undefined;\n\
};\n\
Host.prototype."));
    LPG_TRY(generate_method_name(5, host, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, " = function (value)\n\
{\n\
   return value;\n\
};\n\
Host.prototype."));
    LPG_TRY(generate_method_name(6, host, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, " = function (left, right)\n\
{\n\
   return (left === right);\n\
};\n\
Host.prototype."));
    LPG_TRY(generate_method_name(7, host, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, " = function (from)\n\
{\n\
   return ((typeof from === 'number') && \
isFinite(from) && \
(Math.floor(from) === from)) ? "));
    LPG_TRY(enum_construct_stateful_begin(
        enum_encoding_element_stateful_from_direct(
            ecmascript_value_set_create_integer_range(ecmascript_integer_range_create_any())),
        0, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, "from"));
    LPG_TRY(enum_construct_stateful_end(
        enum_encoding_element_stateful_from_direct(
            ecmascript_value_set_create_integer_range(ecmascript_integer_range_create_any())),
        ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, " : "));
    LPG_TRY(generate_stateless_enum_element(1, ecmascript_output));
    LPG_TRY(stream_writer_write_string(ecmascript_output, ";\n\
};\n\
"));
    return success_yes;
}

lpg_interface const *get_host_interface(checked_program const program)
{
    checked_function const main_ = program.functions[0];
    ASSUME(main_.signature->result.is_set);
    type const main_result = main_.signature->result.value;
    if (main_result.kind != type_kind_lambda)
    {
        return NULL;
    }
    checked_function const entry_point = program.functions[main_result.lambda.lambda];
    if (entry_point.signature->parameters.length != 2)
    {
        return NULL;
    }
    type const host_parameter = entry_point.signature->parameters.elements[1];
    if (host_parameter.kind != type_kind_interface)
    {
        return NULL;
    }
    return program.interfaces + host_parameter.interface_;
}
