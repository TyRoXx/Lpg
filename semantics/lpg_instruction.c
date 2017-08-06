#include "lpg_instruction.h"
#include "lpg_allocate.h"
#include "lpg_for.h"
#include "lpg_assert.h"

tuple_instruction tuple_instruction_create(register_id *elements,
                                           size_t element_count,
                                           register_id result)
{
    tuple_instruction tuple_instruction1 = {elements, element_count, result};
    return tuple_instruction1;
}

call_instruction call_instruction_create(register_id callee,
                                         register_id *arguments,
                                         size_t argument_count,
                                         register_id result)
{
    call_instruction created = {callee, arguments, argument_count, result};
    return created;
}

void call_instruction_free(call_instruction const *value)
{
    if (value->arguments)
    {
        deallocate(value->arguments);
    }
}

bool call_instruction_equals(call_instruction const left,
                             call_instruction const right)
{
    if (left.callee != right.callee)
    {
        return false;
    }
    if (left.result != right.result)
    {
        return false;
    }
    if (left.argument_count != right.argument_count)
    {
        return false;
    }
    LPG_FOR(size_t, i, left.argument_count)
    {
        if (left.arguments[i] != right.arguments[i])
        {
            return false;
        }
    }
    return true;
}

read_struct_instruction read_struct_instruction_create(register_id from_object,
                                                       struct_member_id member,
                                                       register_id into)
{
    read_struct_instruction result = {from_object, member, into};
    return result;
}

bool read_struct_instruction_equals(read_struct_instruction const left,
                                    read_struct_instruction const right)
{
    return (left.from_object == right.from_object) &&
           (left.member == right.member) && (left.into == right.into);
}

literal_instruction literal_instruction_create(register_id into, value value_)
{
    literal_instruction result = {into, value_};
    return result;
}

bool literal_instruction_equals(literal_instruction const left,
                                literal_instruction const right)
{
    return (left.into == right.into) && value_equals(left.value_, right.value_);
}

instruction instruction_create_tuple(tuple_instruction argument)
{
    instruction result;
    result.type = instruction_tuple;
    result.tuple_ = argument;
    return result;
}

instruction instruction_create_call(call_instruction argument)
{
    instruction result;
    result.type = instruction_call;
    result.call = argument;
    return result;
}

instruction instruction_create_global(register_id into)
{
    instruction result;
    result.type = instruction_global;
    result.global_into = into;
    return result;
}

instruction instruction_create_read_struct(read_struct_instruction argument)
{
    instruction result;
    result.type = instruction_read_struct;
    result.read_struct = argument;
    return result;
}

instruction instruction_create_loop(instruction_sequence body)
{
    instruction result;
    result.type = instruction_loop;
    result.loop = body;
    return result;
}

instruction instruction_create_break()
{
    instruction result;
    result.type = instruction_break;
    return result;
}

instruction instruction_create_literal(literal_instruction const value)
{
    instruction result;
    result.type = instruction_literal;
    result.literal = value;
    return result;
}

void instruction_free(instruction const *value)
{
    switch (value->type)
    {
    case instruction_call:
        call_instruction_free(&value->call);
        break;

    case instruction_loop:
        instruction_sequence_free(&value->loop);
        break;

    case instruction_global:
    case instruction_read_struct:
    case instruction_break:
    case instruction_literal:
        break;
    case instruction_tuple:
        deallocate(value->tuple_.elements);
        break;
    }
}

bool instruction_equals(instruction const left, instruction const right)
{
    if (left.type != right.type)
    {
        return false;
    }
    switch (left.type)
    {
    case instruction_call:
        return call_instruction_equals(left.call, right.call);

    case instruction_loop:
        return instruction_sequence_equals(&left.loop, &right.loop);

    case instruction_global:
        return (left.global_into == right.global_into);

    case instruction_read_struct:
        return read_struct_instruction_equals(
            left.read_struct, right.read_struct);

    case instruction_break:
        return true;

    case instruction_literal:
        return (left.literal.into == right.literal.into) &&
               value_equals(left.literal.value_, right.literal.value_);
    case instruction_tuple:
        if (left.tuple_.element_count != right.tuple_.element_count)
        {
            return false;
        }
        for (size_t i = 0; i < left.tuple_.element_count; ++i)
        {
            if (left.tuple_.elements[i] != right.tuple_.elements[i])
            {
                return false;
            }
        }
        return true;
    }
    UNREACHABLE();
}
