#include "lpg_remove_unused_functions.h"
#include "lpg_instruction.h"
#include "lpg_allocate.h"
#include <string.h>
#include "lpg_assert.h"
#include "lpg_structure_member.h"

static void mark_function(bool *used_functions, checked_function const *const all_functions,
                          interface const *const all_interfaces, function_id const marked_function);

static void mark_value(value const root, bool *used_functions, checked_function const *const all_functions,
                       interface const *const all_interfaces)
{
    switch (root.kind)
    {
    case value_kind_type_erased:
        mark_value(*root.type_erased.self, used_functions, all_functions, all_interfaces);
        break;

    case value_kind_function_pointer:
    {
        function_id const referenced = root.function_pointer.code;
        mark_function(used_functions, all_functions, all_interfaces, referenced);
        break;
    }

    case value_kind_integer:
    case value_kind_string:
    case value_kind_flat_object:
    case value_kind_type:
    case value_kind_enum_element:
    case value_kind_unit:
    case value_kind_enum_constructor:
        break;

    case value_kind_tuple:
        for (size_t i = 0; i < root.tuple_.element_count; ++i)
        {
            mark_value(root.tuple_.elements[i], used_functions, all_functions, all_interfaces);
        }
        break;
    }
}

static void mark_used_functions_in_sequence(instruction_sequence const sequence, bool *used_functions,
                                            checked_function const *const all_functions,
                                            interface const *const all_interfaces)
{
    for (size_t j = 0; j < sequence.length; ++j)
    {
        instruction const current_instruction = sequence.elements[j];
        switch (current_instruction.type)
        {
        case instruction_get_method:
            mark_function(used_functions, all_functions, all_interfaces, current_instruction.get_method.method);
            break;

        case instruction_return:
            LPG_TO_DO();

        case instruction_loop:
            mark_used_functions_in_sequence(current_instruction.loop, used_functions, all_functions, all_interfaces);
            break;

        case instruction_call:
        case instruction_global:
        case instruction_read_struct:
        case instruction_break:
        case instruction_erase_type:
        case instruction_tuple:
        case instruction_enum_construct:
        case instruction_get_captures:
            break;

        case instruction_literal:
            mark_value(current_instruction.literal.value_, used_functions, all_functions, all_interfaces);
            break;

        case instruction_match:
            for (size_t k = 0; k < current_instruction.match.count; ++k)
            {
                mark_used_functions_in_sequence(
                    current_instruction.match.cases[k].action, used_functions, all_functions, all_interfaces);
            }
            break;

        case instruction_lambda_with_captures:
            mark_function(
                used_functions, all_functions, all_interfaces, current_instruction.lambda_with_captures.lambda);
            break;
        }
    }
}

static void mark_function(bool *used_functions, checked_function const *const all_functions,
                          interface const *const all_interfaces, function_id const marked_function)
{
    if (used_functions[marked_function])
    {
        return;
    }
    used_functions[marked_function] = true;
    instruction_sequence const sequence = all_functions[marked_function].body;
    mark_used_functions_in_sequence(sequence, used_functions, all_functions, all_interfaces);
}

static function_pointer *clone_function_pointer(function_pointer const original, garbage_collector *const clone_gc)
{
    function_pointer *const result = allocate(sizeof(*result));
    *result = function_pointer_create(
        type_clone(original.result, clone_gc),
        tuple_type_create(allocate_array(original.parameters.length, sizeof(*result->parameters.elements)),
                          original.parameters.length),
        tuple_type_create(
            allocate_array(original.captures.length, sizeof(*result->captures.elements)), original.captures.length),
        optional_type_clone(original.self, clone_gc));
    for (size_t i = 0; i < original.parameters.length; ++i)
    {
        result->parameters.elements[i] = type_clone(original.parameters.elements[i], clone_gc);
    }
    for (size_t i = 0; i < original.captures.length; ++i)
    {
        result->captures.elements[i] = type_clone(original.captures.elements[i], clone_gc);
    }
    return result;
}

static value adapt_value(value const from, garbage_collector *const clone_gc,
                         function_id const *const new_function_ids);

static function_pointer_value clone_function_pointer_value(function_pointer_value const original,
                                                           garbage_collector *const clone_gc,
                                                           function_id const *const new_function_ids)
{
    value *const captures = garbage_collector_allocate_array(clone_gc, original.capture_count, sizeof(*captures));
    for (size_t i = 0; i < original.capture_count; ++i)
    {
        captures[i] = adapt_value(original.captures[i], clone_gc, new_function_ids);
    }
    return function_pointer_value_from_internal(new_function_ids[original.code], captures, original.capture_count);
}

static value adapt_value(value const from, garbage_collector *const clone_gc, function_id const *const new_function_ids)
{
    switch (from.kind)
    {
    case value_kind_integer:
    case value_kind_unit:
        return from;

    case value_kind_string:
    {
        char *const copy = garbage_collector_allocate(clone_gc, from.string_ref.length);
        memcpy(copy, from.string_ref.begin, from.string_ref.length);
        return value_from_string_ref(unicode_view_create(copy, from.string_ref.length));
    }

    case value_kind_function_pointer:
        return value_from_function_pointer(
            clone_function_pointer_value(from.function_pointer, clone_gc, new_function_ids));

    case value_kind_type_erased:
    {
        value *const self = garbage_collector_allocate(clone_gc, sizeof(*self));
        *self = adapt_value(*from.type_erased.self, clone_gc, new_function_ids);
        return value_from_type_erased(type_erased_value_create(from.type_erased.impl, self));
    }

    case value_kind_flat_object:
        LPG_TO_DO();

    case value_kind_type:
        return value_from_type(type_clone(from.type_, clone_gc));

    case value_kind_enum_element:
    {
        value *const state = from.enum_element.state ? garbage_collector_allocate(clone_gc, sizeof(*state)) : NULL;
        if (state)
        {
            *state = adapt_value(*from.enum_element.state, clone_gc, new_function_ids);
        }
        return value_from_enum_element(from.enum_element.which, from.enum_element.state_type, state);
    }

    case value_kind_tuple:
    {
        value *const elements =
            garbage_collector_allocate_array(clone_gc, from.tuple_.element_count, sizeof(*elements));
        for (size_t i = 0; i < from.tuple_.element_count; ++i)
        {
            elements[i] = adapt_value(from.tuple_.elements[i], clone_gc, new_function_ids);
        }
        return value_from_tuple(value_tuple_create(elements, from.tuple_.element_count));
    }

    case value_kind_enum_constructor:
        return value_from_enum_constructor();
    }
    LPG_UNREACHABLE();
}

static optional_value clone_optional_value(optional_value const original, garbage_collector *const clone_gc,
                                           function_id const *const new_function_ids)
{
    if (original.is_set)
    {
        return optional_value_create(adapt_value(original.value_, clone_gc, new_function_ids));
    }
    return original;
}

static instruction_sequence clone_sequence(instruction_sequence const original, garbage_collector *const clone_gc,
                                           function_id const *const new_function_ids);

static instruction clone_instruction(instruction const original, garbage_collector *const clone_gc,
                                     function_id const *const new_function_ids)
{
    switch (original.type)
    {
    case instruction_get_method:
        return instruction_create_get_method(
            get_method_instruction_create(original.get_method.interface_, original.get_method.from,
                                          original.get_method.method, original.get_method.into));

    case instruction_erase_type:
        return instruction_create_erase_type(erase_type_instruction_create(
            original.erase_type.self, original.erase_type.into, original.erase_type.impl));

    case instruction_return:
        LPG_TO_DO();

    case instruction_call:
    {
        register_id *const arguments = allocate_array(original.call.argument_count, sizeof(*arguments));
        memcpy(arguments, original.call.arguments, sizeof(*arguments) * original.call.argument_count);
        return instruction_create_call(call_instruction_create(
            original.call.callee, arguments, original.call.argument_count, original.call.result));
    }

    case instruction_loop:
    {
        instruction *const body = allocate_array(original.loop.length, sizeof(*body));
        for (size_t i = 0; i < original.loop.length; ++i)
        {
            body[i] = clone_instruction(original.loop.elements[i], clone_gc, new_function_ids);
        }
        return instruction_create_loop(instruction_sequence_create(body, original.loop.length));
    }

    case instruction_global:
    case instruction_break:
    case instruction_enum_construct:
    case instruction_get_captures:
        return original;

    case instruction_read_struct:
        ASSUME(original.read_struct.from_object != ~(register_id)0);
        return original;

    case instruction_literal:
        return instruction_create_literal(literal_instruction_create(
            original.literal.into, adapt_value(original.literal.value_, clone_gc, new_function_ids),
            type_clone(original.literal.type_of, clone_gc)));

    case instruction_tuple:
    {
        register_id *const elements = allocate_array(original.tuple_.element_count, sizeof(*elements));
        if (original.tuple_.element_count > 0)
        {
            memcpy(elements, original.tuple_.elements, original.tuple_.element_count * sizeof(*elements));
        }
        tuple_type const cloned_type = {
            allocate_array(original.tuple_.result_type.length, sizeof(*cloned_type.elements)),
            original.tuple_.result_type.length};
        memcpy(cloned_type.elements, original.tuple_.result_type.elements,
               (sizeof(*cloned_type.elements) * original.tuple_.result_type.length));
        return instruction_create_tuple(
            tuple_instruction_create(elements, original.tuple_.element_count, original.tuple_.result, cloned_type));
    }

    case instruction_match:
    {
        match_instruction_case *const cases = allocate_array(original.match.count, sizeof(*cases));
        for (size_t i = 0; i < original.match.count; ++i)
        {
            cases[i] = match_instruction_case_create(
                original.match.cases[i].key, clone_sequence(original.match.cases[i].action, clone_gc, new_function_ids),
                original.match.cases[i].value);
        }
        return instruction_create_match(match_instruction_create(original.match.key, cases, original.match.count,
                                                                 original.match.result,
                                                                 type_clone(original.match.result_type, clone_gc)));
    }

    case instruction_lambda_with_captures:
    {
        register_id *const captures = allocate_array(original.lambda_with_captures.capture_count, sizeof(*captures));
        memcpy(captures, original.lambda_with_captures.captures,
               (original.lambda_with_captures.capture_count * sizeof(*captures)));
        return instruction_create_lambda_with_captures(lambda_with_captures_instruction_create(
            original.lambda_with_captures.into, original.lambda_with_captures.lambda, captures,
            original.lambda_with_captures.capture_count));
    }
    }
    LPG_UNREACHABLE();
}

static instruction_sequence clone_sequence(instruction_sequence const original, garbage_collector *const clone_gc,
                                           function_id const *const new_function_ids)
{
    instruction_sequence result = {allocate_array(original.length, sizeof(*result.elements)), original.length};
    for (size_t i = 0; i < result.length; ++i)
    {
        result.elements[i] = clone_instruction(original.elements[i], clone_gc, new_function_ids);
    }
    return result;
}

static checked_function keep_function(checked_function const original, garbage_collector *const new_gc,
                                      function_id const *const new_function_ids)
{
    unicode_string *const register_debug_names =
        (original.number_of_registers > 0) ? allocate_array(original.number_of_registers, sizeof(*register_debug_names))
                                           : NULL;
    for (size_t i = 0; i < original.number_of_registers; ++i)
    {
        register_debug_names[i] = unicode_view_copy(unicode_view_from_string(original.register_debug_names[i]));
    }
    checked_function const result = checked_function_create(
        original.return_value, clone_function_pointer(*original.signature, new_gc),
        clone_sequence(original.body, new_gc, new_function_ids), register_debug_names, original.number_of_registers);
    return result;
}

static tuple_type clone_tuple_type(tuple_type const original, garbage_collector *const gc)
{
    type *const elements = allocate_array(original.length, sizeof(*elements));
    for (size_t i = 0; i < original.length; ++i)
    {
        elements[i] = type_clone(original.elements[i], gc);
    }
    return tuple_type_create(elements, original.length);
}

static method_description clone_method_description(method_description const original, garbage_collector *const gc)
{
    return method_description_create(unicode_view_copy(unicode_view_from_string(original.name)),
                                     clone_tuple_type(original.parameters, gc), type_clone(original.result, gc));
}

static implementation_entry clone_implementation_entry(implementation_entry const original, garbage_collector *const gc,
                                                       function_id const *const new_function_ids)
{
    function_pointer_value *const methods = allocate_array(original.target.method_count, sizeof(*methods));
    for (size_t i = 0; i < original.target.method_count; ++i)
    {
        methods[i] = clone_function_pointer_value(original.target.methods[i], gc, new_function_ids);
    }
    return implementation_entry_create(
        type_clone(original.self, gc), implementation_create(methods, original.target.method_count));
}

static interface clone_interface(interface const original, garbage_collector *const gc,
                                 function_id const *const new_function_ids)
{
    method_description *const methods = allocate_array(original.method_count, sizeof(*methods));
    for (interface_id i = 0; i < original.method_count; ++i)
    {
        methods[i] = clone_method_description(original.methods[i], gc);
    }
    implementation_entry *const implementations =
        allocate_array(original.implementation_count, sizeof(*implementations));
    for (interface_id i = 0; i < original.implementation_count; ++i)
    {
        implementations[i] = clone_implementation_entry(original.implementations[i], gc, new_function_ids);
    }
    return interface_create(methods, original.method_count, implementations, original.implementation_count);
}

static interface *clone_interfaces(interface *const interfaces, const interface_id interface_count,
                                   garbage_collector *const gc, function_id const *const new_function_ids)
{
    interface *const result = allocate_array(interface_count, sizeof(*result));
    for (interface_id i = 0; i < interface_count; ++i)
    {
        result[i] = clone_interface(interfaces[i], gc, new_function_ids);
    }
    return result;
}

static structure clone_structure(structure const original, garbage_collector *const gc,
                                 function_id const *const new_function_ids)
{
    structure_member *const members = allocate_array(original.count, sizeof(*members));
    for (size_t i = 0; i < original.count; ++i)
    {
        members[i] =
            structure_member_create(type_clone(original.members[i].what, gc),
                                    unicode_view_copy(unicode_view_from_string(original.members[i].name)),
                                    clone_optional_value(original.members[i].compile_time_value, gc, new_function_ids));
    }
    return structure_create(members, original.count);
}

static structure *clone_structures(structure const *const structures, struct_id const structure_count,
                                   garbage_collector *const gc, function_id const *const new_function_ids)
{
    structure *const result = allocate_array(structure_count, sizeof(*result));
    for (interface_id i = 0; i < structure_count; ++i)
    {
        result[i] = clone_structure(structures[i], gc, new_function_ids);
    }
    return result;
}

checked_program remove_unused_functions(checked_program const from)
{
    bool *const used_functions = allocate_array(from.function_count, sizeof(*used_functions));
    memset(used_functions, 0, sizeof(*used_functions) * from.function_count);
    mark_function(used_functions, from.functions, from.interfaces, 0);

    for (interface_id i = 0; i < from.interface_count; ++i)
    {
        for (size_t k = 0; k < from.interfaces[i].implementation_count; ++k)
        {
            implementation const impl = from.interfaces[i].implementations[k].target;
            for (function_id m = 0; m < impl.method_count; ++m)
            {
                mark_function(used_functions, from.functions, from.interfaces, impl.methods[m].code);
            }
        }
    }

    function_id *const new_function_ids = allocate_array(from.function_count, sizeof(*new_function_ids));
    function_id next_new_function_id = 0;
    for (function_id i = 0; i < from.function_count; ++i)
    {
        if (used_functions[i])
        {
            new_function_ids[i] = next_new_function_id;
            ++next_new_function_id;
        }
        else
        {
            new_function_ids[i] = ~(function_id)0;
        }
    }
    function_id const new_function_count = next_new_function_id;
    checked_program result = {NULL,
                              from.interface_count,
                              NULL,
                              from.struct_count,
                              {NULL},
                              allocate_array(new_function_count, sizeof(*result.functions)),
                              new_function_count};
    result.interfaces = clone_interfaces(from.interfaces, from.interface_count, &result.memory, new_function_ids);
    result.structs = clone_structures(from.structs, from.struct_count, &result.memory, new_function_ids);
    for (function_id i = 0; i < from.function_count; ++i)
    {
        if (!used_functions[i])
        {
            continue;
        }
        checked_function *const new_function = result.functions + new_function_ids[i];
        *new_function = keep_function(from.functions[i], &result.memory, new_function_ids);
    }
    deallocate(used_functions);
    deallocate(new_function_ids);
    ASSUME(from.interface_count == result.interface_count);
    return result;
}
