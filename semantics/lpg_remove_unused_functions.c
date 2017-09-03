#include "lpg_remove_unused_functions.h"
#include "lpg_instruction.h"
#include "lpg_allocate.h"
#include <string.h>
#include "lpg_assert.h"

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
    for (size_t j = 0; j < sequence.length; ++j)
    {
        instruction const current_instruction = sequence.elements[j];
        switch (current_instruction.type)
        {
        case instruction_call:
            break;

        case instruction_loop:
            break;

        case instruction_global:
            break;

        case instruction_read_struct:
            break;

        case instruction_break:
            break;

        case instruction_literal:
            switch (current_instruction.literal.value_.kind)
            {
            case value_kind_integer:
                break;

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
                break;

            case value_kind_type:
                break;

            case value_kind_enum_element:
                break;

            case value_kind_unit:
                break;

            case value_kind_tuple:
                break;

            case value_kind_enum_constructor:
                break;
            }
            break;

        case instruction_tuple:
            break;

        case instruction_enum_construct:
            break;
        }
    }
}

static function_pointer *
clone_function_pointer(function_pointer const original,
                       garbage_collector *const clone_gc)
{
    function_pointer *const result = allocate(sizeof(*result));
    result->result = type_clone(original.result, clone_gc);
    result->arity = original.arity;
    result->arguments =
        allocate_array(original.arity, sizeof(*result->arguments));
    for (size_t i = 0; i < original.arity; ++i)
    {
        result->arguments[i] = type_clone(original.arguments[i], clone_gc);
    }
    return result;
}

static instruction clone_instruction(instruction const original,
                                     garbage_collector *const clone_gc)
{
    switch (original.type)
    {
    case instruction_call:
    {
        register_id *const arguments = garbage_collector_allocate_array(
            clone_gc, original.call.argument_count, sizeof(*arguments));
        memcpy(arguments, original.call.arguments,
               sizeof(*arguments) * original.call.argument_count);
        return instruction_create_call(call_instruction_create(
            original.call.callee, arguments, original.call.argument_count,
            original.call.result));
    }

    case instruction_loop:
        LPG_TO_DO();

    case instruction_global:
        return original;

    case instruction_read_struct:
        return original;

    case instruction_break:
        return original;

    case instruction_literal:
        LPG_TO_DO();

    case instruction_tuple:
        LPG_TO_DO();

    case instruction_enum_construct:
        return original;
    }
    LPG_UNREACHABLE();
}

static instruction_sequence clone_sequence(instruction_sequence const original,
                                           garbage_collector *const clone_gc)
{
    instruction_sequence result = {
        allocate_array(original.length, sizeof(*result.elements)),
        original.length};
    for (size_t i = 0; i < result.length; ++i)
    {
        result.elements[i] = clone_instruction(original.elements[i], clone_gc);
    }
    return result;
}

static checked_function keep_function(checked_function const original,
                                      garbage_collector *const new_gc)
{
    checked_function result = {
        original.return_value,
        clone_function_pointer(*original.signature, new_gc),
        clone_sequence(original.body, new_gc), original.number_of_registers};
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
        *new_function = keep_function(from.functions[i], &result.memory);
    }
    deallocate(used_functions);
    deallocate(new_function_ids);
    return result;
}
