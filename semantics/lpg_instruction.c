#include "lpg_instruction.h"
#include "lpg_allocate.h"
#include "lpg_for.h"
#include "lpg_assert.h"

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

string_literal_instruction
string_literal_instruction_create(unicode_string value, register_id into)
{
    string_literal_instruction result = {value, into};
    return result;
}

void string_literal_instruction_free(string_literal_instruction const *value)
{
    unicode_string_free(&value->value);
}

bool string_literal_instruction_equals(string_literal_instruction const left,
                                       string_literal_instruction const right)
{
    return unicode_string_equals(left.value, right.value) &&
           (left.into == right.into);
}

instantiate_enum_instruction
instantiate_enum_instruction_create(register_id into, enum_element_id element)
{
    instantiate_enum_instruction result = {into, element};
    return result;
}

bool instantiate_enum_instruction_equals(
    instantiate_enum_instruction const left,
    instantiate_enum_instruction const right)
{
    return (left.into == right.into) && (left.element == right.element);
}

integer_literal_instruction integer_literal_instruction_create(register_id into,
                                                               integer value)
{
    integer_literal_instruction result = {into, value};
    return result;
}

bool integer_literal_instruction_equals(integer_literal_instruction const left,
                                        integer_literal_instruction const right)
{
    return (left.into == right.into) && integer_equal(left.value, right.value);
}

literal_instruction literal_instruction_create(register_id into, value value_)
{
    literal_instruction result = {into, value_};
    return result;
}

bool literal_instruction_equals(literal_instruction const left,
                                literal_instruction const right)
{
    return (left.into == right.into); /*TODO: compare values*/
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

instruction instruction_create_unit(register_id into)
{
    instruction result;
    result.type = instruction_unit;
    result.unit = into;
    return result;
}

instruction instruction_create_string_literal(string_literal_instruction value)
{
    instruction result;
    result.type = instruction_string_literal;
    result.string_literal = value;
    return result;
}

instruction instruction_create_break()
{
    instruction result;
    result.type = instruction_break;
    return result;
}

instruction
instruction_create_instantiate_enum(instantiate_enum_instruction value)
{
    instruction result;
    result.type = instruction_instantiate_enum;
    result.instantiate_enum = value;
    return result;
}

instruction
instruction_create_integer_literal(integer_literal_instruction const value)
{
    instruction result;
    result.type = instruction_integer_literal;
    result.integer = value;
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
        break;

    case instruction_read_struct:
        break;

    case instruction_unit:
        break;

    case instruction_string_literal:
        string_literal_instruction_free(&value->string_literal);
        break;

    case instruction_break:
        break;

    case instruction_instantiate_enum:
        break;

    case instruction_integer_literal:
        break;

    case instruction_literal:
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

    case instruction_unit:
        return (left.unit == right.unit);

    case instruction_string_literal:
        return string_literal_instruction_equals(
            left.string_literal, right.string_literal);

    case instruction_break:
        return true;

    case instruction_instantiate_enum:
        return instantiate_enum_instruction_equals(
            left.instantiate_enum, right.instantiate_enum);

    case instruction_integer_literal:
        return integer_literal_instruction_equals(left.integer, right.integer);

    case instruction_literal:
        return (left.integer.into == right.integer.into) &&
               value_equals(left.literal.value_, right.literal.value_);
    }
    UNREACHABLE();
}
