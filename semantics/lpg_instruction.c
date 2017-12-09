#include "lpg_instruction.h"
#include "lpg_allocate.h"
#include "lpg_for.h"

tuple_instruction tuple_instruction_create(register_id *elements, size_t element_count, register_id result,
                                           tuple_type result_type)
{
    tuple_instruction const tuple_instruction1 = {elements, element_count, result, result_type};
    return tuple_instruction1;
}

call_instruction call_instruction_create(register_id callee, register_id *arguments, size_t argument_count,
                                         register_id result)
{
    call_instruction const created = {callee, arguments, argument_count, result};
    return created;
}

void call_instruction_free(call_instruction const *value)
{
    if (value->arguments)
    {
        deallocate(value->arguments);
    }
}

bool call_instruction_equals(call_instruction const left, call_instruction const right)
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

return_instruction return_instruction_create(register_id result_register)
{
    return_instruction const result = {result_register};
    return result;
}

read_struct_instruction read_struct_instruction_create(register_id from_object, struct_member_id member,
                                                       register_id into)
{
    read_struct_instruction const result = {from_object, member, into};
    return result;
}

bool read_struct_instruction_equals(read_struct_instruction const left, read_struct_instruction const right)
{
    return (left.from_object == right.from_object) && (left.member == right.member) && (left.into == right.into);
}

literal_instruction literal_instruction_create(register_id into, value value_, type type_of)
{
    literal_instruction const result = {into, value_, type_of};
    return result;
}

bool literal_instruction_equals(literal_instruction const left, literal_instruction const right)
{
    return (left.into == right.into) && value_equals(left.value_, right.value_) &&
           type_equals(left.type_of, right.type_of);
}

enum_construct_instruction enum_construct_instruction_create(register_id into, enum_element_id which, register_id state,
                                                             type state_type)
{
    enum_construct_instruction const result = {into, which, state, state_type};
    return result;
}

bool enum_construct_instruction_equals(enum_construct_instruction const left, enum_construct_instruction const right)
{
    return (left.into == right.into) && (left.which == right.which) && (left.state == right.state) &&
           type_equals(left.state_type, right.state_type);
}

match_instruction match_instruction_create(register_id key, match_instruction_case *cases, size_t count,
                                           register_id result, type result_type)
{
    match_instruction const returning = {key, cases, count, result, result_type};
    return returning;
}

lambda_with_captures_instruction lambda_with_captures_instruction_create(register_id into, function_id lambda,
                                                                         register_id *captures, size_t capture_count)
{
    lambda_with_captures_instruction const result = {into, lambda, captures, capture_count};
    return result;
}

void lambda_with_captures_instruction_free(lambda_with_captures_instruction const freed)
{
    if (freed.captures)
    {
        deallocate(freed.captures);
    }
}

bool lambda_with_captures_instruction_equals(lambda_with_captures_instruction const left,
                                             lambda_with_captures_instruction const right)
{
    if (left.into != right.into)
    {
        return false;
    }
    if (left.lambda != right.lambda)
    {
        return false;
    }
    if (left.capture_count != right.capture_count)
    {
        return false;
    }
    for (size_t i = 0; i < left.capture_count; ++i)
    {
        if (left.captures[i] != right.captures[i])
        {
            return false;
        }
    }
    return true;
}

get_method_instruction get_method_instruction_create(interface_id interface_, register_id from, function_id method,
                                                     register_id into)
{
    get_method_instruction const result = {interface_, from, method, into};
    return result;
}

bool get_method_instruction_equals(get_method_instruction const left, get_method_instruction const right)
{
    return (left.interface_ == right.interface_) && (left.from == right.from) && (left.method == right.method) &&
           (left.into == right.into);
}

erase_type_instruction erase_type_instruction_create(register_id self, register_id into, implementation_ref impl)
{
    erase_type_instruction const result = {self, into, impl};
    return result;
}

bool erase_type_instruction_equals(erase_type_instruction const left, erase_type_instruction const right)
{
    return (left.self == right.self) && (left.into == right.into) && implementation_ref_equals(left.impl, right.impl);
}

instruction instruction_create_tuple(tuple_instruction argument)
{
    instruction result;
    result.type = instruction_tuple;
    result.tuple_ = argument;
    return result;
}

instruction instruction_create_enum_construct(enum_construct_instruction argument)
{
    instruction result;
    result.type = instruction_enum_construct;
    result.enum_construct = argument;
    return result;
}

instruction instruction_create_match(match_instruction argument)
{
    instruction result;
    result.type = instruction_match;
    result.match = argument;
    return result;
}

instruction instruction_create_get_captures(register_id const into)
{
    instruction result;
    result.type = instruction_get_captures;
    result.captures = into;
    return result;
}

instruction instruction_create_lambda_with_captures(lambda_with_captures_instruction const argument)
{
    instruction result;
    result.type = instruction_lambda_with_captures;
    result.lambda_with_captures = argument;
    return result;
}

instruction instruction_create_get_method(get_method_instruction const argument)
{
    instruction result;
    result.type = instruction_get_method;
    result.get_method = argument;
    return result;
}

instruction instruction_create_erase_type(erase_type_instruction const argument)
{
    instruction result;
    result.type = instruction_erase_type;
    result.erase_type = argument;
    return result;
}

match_instruction_case match_instruction_case_create(register_id key, instruction_sequence action, register_id value)
{
    match_instruction_case const result = {key, action, value};
    return result;
}

void match_instruction_case_free(match_instruction_case freed)
{
    instruction_sequence_free(&freed.action);
}

instruction instruction_create_call(call_instruction argument)
{
    instruction result;
    result.type = instruction_call;
    result.call = argument;
    return result;
}

instruction instruction_create_return(return_instruction argument)
{
    instruction result;
    result.type = instruction_return;
    result.return_ = argument;
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

instruction instruction_create_break(register_id const into)
{
    instruction result;
    result.type = instruction_break;
    result.break_into = into;
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

    case instruction_get_method:
    case instruction_global:
    case instruction_read_struct:
    case instruction_break:
    case instruction_literal:
    case instruction_return:
    case instruction_get_captures:
    case instruction_enum_construct:
    case instruction_erase_type:
        break;

    case instruction_tuple:
        deallocate(value->tuple_.elements);
        deallocate(value->tuple_.result_type.elements);
        break;

    case instruction_match:
        for (size_t i = 0; i < value->match.count; ++i)
        {
            match_instruction_case_free(value->match.cases[i]);
        }
        if (value->match.cases)
        {
            deallocate(value->match.cases);
        }
        break;

    case instruction_lambda_with_captures:
        lambda_with_captures_instruction_free(value->lambda_with_captures);
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
    case instruction_get_method:
        LPG_TO_DO();

    case instruction_call:
        return call_instruction_equals(left.call, right.call);

    case instruction_return:
        return left.return_.return_register == right.return_.return_register;

    case instruction_loop:
        return instruction_sequence_equals(&left.loop, &right.loop);

    case instruction_global:
        return (left.global_into == right.global_into);

    case instruction_read_struct:
        return read_struct_instruction_equals(left.read_struct, right.read_struct);

    case instruction_break:
        return (left.break_into == right.break_into);

    case instruction_literal:
        return (left.literal.into == right.literal.into) && value_equals(left.literal.value_, right.literal.value_);

    case instruction_tuple:
        return left.tuple_.result == right.tuple_.result;

    case instruction_enum_construct:
        LPG_TO_DO();

    case instruction_match:
        if (left.match.key != right.match.key)
        {
            return false;
        }
        if (left.match.result != right.match.result)
        {
            return false;
        }
        if (left.match.count != right.match.count)
        {
            return false;
        }
        for (size_t i = 0; i < left.match.count; ++i)
        {
            if (left.match.cases[i].key != right.match.cases[i].key)
            {
                return false;
            }
            if (!instruction_sequence_equals(&left.match.cases[i].action, &right.match.cases[i].action))
            {
                return false;
            }
            if (left.match.cases[i].value != right.match.cases[i].value)
            {
                return false;
            }
        }
        return true;

    case instruction_get_captures:
        return true;

    case instruction_lambda_with_captures:
        return lambda_with_captures_instruction_equals(left.lambda_with_captures, right.lambda_with_captures);

    case instruction_erase_type:
        return erase_type_instruction_equals(left.erase_type, right.erase_type);
    }
    LPG_UNREACHABLE();
}

void add_instruction(instruction_sequence *to, instruction const added)
{
    to->elements = reallocate_array(to->elements, (to->length + 1), sizeof(*to->elements));
    to->elements[to->length] = added;
    ++(to->length);
}
