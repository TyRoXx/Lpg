#include "lpg_remove_unused_functions.h"
#include "lpg_instruction.h"
#include "lpg_allocate.h"
#include <string.h>
#include "lpg_assert.h"

static void mark_function(bool *used_functions,
                          checked_function const *const all_functions,
                          function_id const marked_function);

static void
mark_used_functions_in_sequence(instruction_sequence const sequence,
                                bool *used_functions,
                                checked_function const *const all_functions)
{
    for (size_t j = 0; j < sequence.length; ++j)
    {
        instruction const current_instruction = sequence.elements[j];
        switch (current_instruction.type)
        {
        case instruction_call:
            break;

        case instruction_loop:
            mark_used_functions_in_sequence(
                current_instruction.loop, used_functions, all_functions);
            break;

        case instruction_global:
        case instruction_read_struct:
        case instruction_break:
            break;

        case instruction_literal:
            switch (current_instruction.literal.value_.kind)
            {
            case value_kind_integer:
            case value_kind_string:
                break;

            case value_kind_function_pointer:
            {
                function_id const referenced =
                    current_instruction.literal.value_.function_pointer.code;
                mark_function(used_functions, all_functions, referenced);
                break;
            }

            case value_kind_flat_object:
            case value_kind_type:
            case value_kind_enum_element:
            case value_kind_unit:
            case value_kind_tuple:
            case value_kind_enum_constructor:
                break;
            }
            break;

        case instruction_tuple:
        case instruction_enum_construct:
            break;

        case instruction_match:
            for (size_t k = 0; k < current_instruction.match.count; ++k)
            {
                mark_used_functions_in_sequence(
                    current_instruction.match.cases[k].action, used_functions,
                    all_functions);
            }
            break;
        }
    }
}

static void mark_function(bool *used_functions,
                          checked_function const *const all_functions,
                          function_id const marked_function)
{
    if (used_functions[marked_function])
    {
        return;
    }
    used_functions[marked_function] = true;
    instruction_sequence const sequence = all_functions[marked_function].body;
    mark_used_functions_in_sequence(sequence, used_functions, all_functions);
}

static function_pointer *
clone_function_pointer(function_pointer const original,
                       garbage_collector *const clone_gc)
{
    function_pointer *const result = allocate(sizeof(*result));
    result->result = type_clone(original.result, clone_gc);
    result->parameters.length = original.parameters.length;
    result->parameters.elements = allocate_array(
        original.parameters.length, sizeof(*result->parameters.elements));
    for (size_t i = 0; i < original.parameters.length; ++i)
    {
        result->parameters.elements[i] =
            type_clone(original.parameters.elements[i], clone_gc);
    }
    return result;
}

static value adapt_value(value const from, garbage_collector *const clone_gc,
                         function_id const *const new_function_ids)
{
    switch (from.kind)
    {
    case value_kind_integer:
        return from;

    case value_kind_string:
    {
        char *const copy =
            garbage_collector_allocate(clone_gc, from.string_ref.length);
        memcpy(copy, from.string_ref.begin, from.string_ref.length);
        return value_from_string_ref(
            unicode_view_create(copy, from.string_ref.length));
    }

    case value_kind_function_pointer:
        return value_from_function_pointer(function_pointer_value_from_internal(
            new_function_ids[from.function_pointer.code]));

    case value_kind_flat_object:
    case value_kind_type:
        LPG_TO_DO();

    case value_kind_enum_element:
    {
        value *const state =
            from.enum_element.state ? allocate(sizeof(*state)) : NULL;
        if (state)
        {
            *state = adapt_value(
                *from.enum_element.state, clone_gc, new_function_ids);
        }
        return value_from_enum_element(from.enum_element.which, state);
    }

    case value_kind_unit:
        return from;

    case value_kind_tuple:
    case value_kind_enum_constructor:
        LPG_TO_DO();
    }
    LPG_UNREACHABLE();
}

static instruction_sequence
clone_sequence(instruction_sequence const original,
               garbage_collector *const clone_gc,
               function_id const *const new_function_ids);

static instruction clone_instruction(instruction const original,
                                     garbage_collector *const clone_gc,
                                     function_id const *const new_function_ids)
{
    switch (original.type)
    {
    case instruction_call:
    {
        register_id *const arguments =
            allocate_array(original.call.argument_count, sizeof(*arguments));
        memcpy(arguments, original.call.arguments,
               sizeof(*arguments) * original.call.argument_count);
        return instruction_create_call(call_instruction_create(
            original.call.callee, arguments, original.call.argument_count,
            original.call.result));
    }

    case instruction_loop:
    {
        instruction *const body =
            allocate_array(original.loop.length, sizeof(*body));
        for (size_t i = 0; i < original.loop.length; ++i)
        {
            body[i] = clone_instruction(
                original.loop.elements[i], clone_gc, new_function_ids);
        }
        return instruction_create_loop(
            instruction_sequence_create(body, original.loop.length));
    }

    case instruction_global:
        return original;

    case instruction_read_struct:
        return original;

    case instruction_break:
        return original;

    case instruction_literal:
        return instruction_create_literal(literal_instruction_create(
            original.literal.into,
            adapt_value(original.literal.value_, clone_gc, new_function_ids),
            type_clone(original.literal.type_of, clone_gc)));

    case instruction_tuple:
        LPG_TO_DO();

    case instruction_enum_construct:
        return original;

    case instruction_match:
    {
        match_instruction_case *const cases =
            allocate_array(original.match.count, sizeof(*cases));
        for (size_t i = 0; i < original.match.count; ++i)
        {
            cases[i] = match_instruction_case_create(
                original.match.cases[i].key,
                clone_sequence(
                    original.match.cases[i].action, clone_gc, new_function_ids),
                original.match.cases[i].value);
        }
        return instruction_create_match(match_instruction_create(
            original.match.key, cases, original.match.count,
            original.match.result,
            type_clone(original.match.result_type, clone_gc)));
    }
    }
    LPG_UNREACHABLE();
}

static instruction_sequence
clone_sequence(instruction_sequence const original,
               garbage_collector *const clone_gc,
               function_id const *const new_function_ids)
{
    instruction_sequence result = {
        allocate_array(original.length, sizeof(*result.elements)),
        original.length};
    for (size_t i = 0; i < result.length; ++i)
    {
        result.elements[i] =
            clone_instruction(original.elements[i], clone_gc, new_function_ids);
    }
    return result;
}

static checked_function keep_function(checked_function const original,
                                      garbage_collector *const new_gc,
                                      function_id const *const new_function_ids)
{
    checked_function result = {
        original.return_value,
        clone_function_pointer(*original.signature, new_gc),
        clone_sequence(original.body, new_gc, new_function_ids),
        original.number_of_registers};
    return result;
}

checked_program remove_unused_functions(checked_program const from)
{
    bool *const used_functions =
        allocate_array(from.function_count, sizeof(*used_functions));
    memset(used_functions, 0, sizeof(*used_functions) * from.function_count);
    mark_function(used_functions, from.functions, 0);
    function_id *const new_function_ids =
        allocate_array(from.function_count, sizeof(*new_function_ids));
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
    checked_program result = {
        {NULL},
        allocate_array(new_function_count, sizeof(*result.functions)),
        new_function_count};
    for (function_id i = 0; i < from.function_count; ++i)
    {
        if (!used_functions[i])
        {
            continue;
        }
        checked_function *const new_function =
            result.functions + new_function_ids[i];
        *new_function =
            keep_function(from.functions[i], &result.memory, new_function_ids);
    }
    deallocate(used_functions);
    deallocate(new_function_ids);
    return result;
}
