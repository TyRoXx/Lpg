#include "lpg_remove_dead_code.h"
#include "lpg_instruction.h"
#include "lpg_assert.h"
#include "lpg_allocate.h"
#include <string.h>

static void find_used_registers(instruction_sequence const from,
                                bool *const registers_read_from)
{
    for (size_t i = 0; i < from.length; ++i)
    {
        instruction const current_instruction =
            from.elements[from.length - i - 1u];
        switch (current_instruction.type)
        {
        case instruction_call:
            registers_read_from[current_instruction.call.callee] = true;
            for (size_t j = 0; j < current_instruction.call.argument_count; ++j)
            {
                registers_read_from[current_instruction.call.arguments[j]] =
                    true;
            }
            /*never remove function calls because they might have side effects*/
            registers_read_from[current_instruction.call.result] = true;
            break;

        case instruction_loop:
            find_used_registers(current_instruction.loop, registers_read_from);
            break;

        case instruction_global:
            break;

        case instruction_read_struct:
            registers_read_from[current_instruction.read_struct.from_object] =
                true;
            break;

        case instruction_break:
        case instruction_literal:
            break;

        case instruction_tuple:
            for (size_t j = 0; j < current_instruction.tuple_.element_count;
                 ++j)
            {
                registers_read_from[current_instruction.tuple_.elements[j]] =
                    true;
            }
            break;

        case instruction_enum_construct:
            registers_read_from[current_instruction.enum_construct.state] =
                true;
            break;

        case instruction_match:
            registers_read_from[current_instruction.match.key] = true;
            for (size_t j = 0; j < current_instruction.match.count; ++j)
            {
                registers_read_from[current_instruction.match.cases[j].key] =
                    true;
                registers_read_from[current_instruction.match.cases[j].value] =
                    true;
            }
            break;
        }
    }
}

static register_id const no_register = ~(register_id)0;

static bool update_register_id(register_id *const updated,
                               register_id const *const new_register_ids)
{
    ASSUME(*updated != no_register);
    *updated = new_register_ids[*updated];
    return (*updated != no_register);
}

static void
change_register_ids_in_sequence(instruction_sequence *const sequence,
                                register_id const *const new_register_ids);

static bool change_register_ids(instruction *const where,
                                register_id const *const new_register_ids)
{
    switch (where->type)
    {
    case instruction_call:
        ASSERT(update_register_id(&where->call.callee, new_register_ids));
        for (size_t j = 0; j < where->call.argument_count; ++j)
        {
            ASSERT(update_register_id(
                where->call.arguments + j, new_register_ids));
        }
        ASSERT(update_register_id(&where->call.result, new_register_ids));
        /*never remove function calls because they might have side effects*/
        return true;

    case instruction_loop:
        change_register_ids_in_sequence(&where->loop, new_register_ids);
        return (where->loop.length >= 1);

    case instruction_global:
        return update_register_id(&where->global_into, new_register_ids);

    case instruction_read_struct:
        ASSERT(update_register_id(
            &where->read_struct.from_object, new_register_ids));
        return update_register_id(&where->read_struct.into, new_register_ids);

    case instruction_break:
        return true;

    case instruction_literal:
        return update_register_id(&where->literal.into, new_register_ids);

    case instruction_tuple:
        for (size_t j = 0; j < where->tuple_.element_count; ++j)
        {
            ASSERT(update_register_id(
                where->tuple_.elements + j, new_register_ids));
        }
        return update_register_id(&where->tuple_.result, new_register_ids);

    case instruction_enum_construct:
        ASSERT(
            update_register_id(&where->enum_construct.state, new_register_ids));
        return update_register_id(
            &where->enum_construct.into, new_register_ids);

    case instruction_match:
        ASSERT(update_register_id(&where->match.key, new_register_ids));
        for (size_t j = 0; j < where->match.count; ++j)
        {
            ASSERT(update_register_id(
                &where->match.cases[j].key, new_register_ids));
            ASSERT(update_register_id(
                &where->match.cases[j].value, new_register_ids));
            change_register_ids_in_sequence(
                &where->match.cases[j].action, new_register_ids);
        }
        return update_register_id(&where->match.result, new_register_ids);
    }
    LPG_UNREACHABLE();
}

static void
change_register_ids_in_sequence(instruction_sequence *const sequence,
                                register_id const *const new_register_ids)
{
    size_t used_until = 0;
    for (size_t i = 0; i < sequence->length; ++i)
    {
        if (change_register_ids(sequence->elements + i, new_register_ids))
        {
            sequence->elements[used_until] = sequence->elements[i];
            ++used_until;
        }
        else
        {
            instruction_free(sequence->elements + i);
        }
    }
    sequence->length = used_until;
}

typedef enum removed_something
{
    removed_something_yes,
    removed_something_no
} removed_something;

static removed_something
remove_one_layer_of_dead_code_from_function(checked_function *const from)
{
    bool *const registers_read_from =
        allocate_array(from->number_of_registers, sizeof(*registers_read_from));
    memset(registers_read_from, 0,
           from->number_of_registers * sizeof(*registers_read_from));
    registers_read_from[from->return_value] = true;
    for (size_t i = 0; i < from->signature->parameters.length; ++i)
    {
        registers_read_from[i] = true;
    }
    find_used_registers(from->body, registers_read_from);
    register_id *const new_register_ids =
        allocate_array(from->number_of_registers, sizeof(*new_register_ids));
    removed_something result = removed_something_no;
    {
        register_id next_register = 0;
        for (register_id i = 0; i < from->number_of_registers; ++i)
        {
            if (registers_read_from[i])
            {
                new_register_ids[i] = next_register;
                ++next_register;
            }
            else
            {
                new_register_ids[i] = no_register;
                result = removed_something_yes;
            }
        }
        from->number_of_registers = next_register;
    }
    deallocate(registers_read_from);
    update_register_id(&from->return_value, new_register_ids);
    change_register_ids_in_sequence(&from->body, new_register_ids);
    deallocate(new_register_ids);
    return result;
}

static void remove_dead_code_from_function(checked_function *const from)
{
    for (;;)
    {
        switch (remove_one_layer_of_dead_code_from_function(from))
        {
        case removed_something_no:
            return;

        case removed_something_yes:
            break;
        }
    }
}

void remove_dead_code(checked_program *const from)
{
    for (size_t i = 0; i < from->function_count; ++i)
    {
        remove_dead_code_from_function(from->functions + i);
    }
}
