#include "lpg_instruction.h"
#include "lpg_allocate.h"
#include "lpg_for.h"
#include <string.h>

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

void call_instruction_free(call_instruction const *freed)
{
    if (freed->arguments)
    {
        deallocate(freed->arguments);
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

return_instruction return_instruction_create(register_id returned_value, register_id unit_goes_into)
{
    return_instruction const result = {returned_value, unit_goes_into};
    return result;
}

bool return_instruction_equals(return_instruction const left, return_instruction const right)
{
    return (left.returned_value == right.returned_value) && (left.unit_goes_into == right.unit_goes_into);
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
    ASSUME(!value_is_mutable(value_));
    ASSUME(value_conforms_to_type(value_, type_of));
    literal_instruction const result = {into, value_, type_of};
    return result;
}

bool literal_instruction_equals(literal_instruction const left, literal_instruction const right)
{
    return (left.into == right.into) && value_equals(left.value_, right.value_) &&
           type_equals(left.type_of, right.type_of);
}

enum_construct_instruction enum_construct_instruction_create(register_id into, enum_constructor_type which,
                                                             register_id state, type state_type)
{
    enum_construct_instruction const result = {into, which, state, state_type};
    return result;
}

bool enum_construct_instruction_equals(enum_construct_instruction const left, enum_construct_instruction const right)
{
    return (left.into == right.into) && enum_constructor_type_equals(left.which, right.which) &&
           (left.state == right.state) && type_equals(left.state_type, right.state_type);
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

instantiate_struct_instruction instantiate_struct_instruction_create(register_id into, struct_id instantiated,
                                                                     register_id *arguments, size_t argument_count)
{
    instantiate_struct_instruction const result = {into, instantiated, arguments, argument_count};
    return result;
}

void instantiate_struct_instruction_free(instantiate_struct_instruction const freed)
{
    if (freed.arguments)
    {
        deallocate(freed.arguments);
    }
}

bool instantiate_struct_instruction_equals(instantiate_struct_instruction const left,
                                           instantiate_struct_instruction const right)
{
    return (left.into == right.into) && (left.instantiated == right.instantiated) &&
           !memcmp(left.arguments, right.arguments, sizeof(*left.arguments) * left.argument_count);
}

loop_instruction loop_instruction_create(register_id unit_goes_into, instruction_sequence body)
{
    loop_instruction const result = {unit_goes_into, body};
    return result;
}

void loop_instruction_free(loop_instruction const freed)
{
    instruction_sequence_free(&freed.body);
}

bool loop_instruction_equals(loop_instruction const left, loop_instruction const right)
{
    return instruction_sequence_equals(&left.body, &right.body) && (left.unit_goes_into == right.unit_goes_into);
}

new_array_instruction new_array_instruction_create(interface_id result_type, register_id into, type element_type)
{
    new_array_instruction const result = {result_type, into, element_type};
    return result;
}

void new_array_instruction_free(new_array_instruction const freed)
{
    (void)freed;
}

bool new_array_instruction_equals(new_array_instruction const left, new_array_instruction const right)
{
    return (left.result_type == right.result_type) && (left.into == right.into) &&
           type_equals(left.element_type, right.element_type);
}

current_function_instruction current_function_instruction_create(register_id into)
{
    current_function_instruction const result = {into};
    return result;
}

void current_function_instruction_free(current_function_instruction const freed)
{
    (void)freed;
}

bool current_function_instruction_equals(current_function_instruction const left,
                                         current_function_instruction const right)
{
    return (left.into == right.into);
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

instruction instruction_create_instantiate_struct(instantiate_struct_instruction const argument)
{
    instruction result;
    result.type = instruction_instantiate_struct;
    result.instantiate_struct = argument;
    return result;
}

instruction instruction_create_new_array(new_array_instruction const argument)
{
    instruction result;
    result.type = instruction_new_array;
    result.new_array = argument;
    return result;
}

instruction instruction_create_current_function(current_function_instruction const argument)
{
    instruction result;
    result.type = instruction_current_function;
    result.current_function = argument;
    return result;
}

match_instruction_case_stateful_enum match_instruction_case_stateful_enum_create(enum_element_id element,
                                                                                 register_id where)
{
    ASSUME(where != ~(register_id)0);
    match_instruction_case_stateful_enum const result = {element, where};
    return result;
}

match_instruction_case match_instruction_case_create_value(register_id key_value, instruction_sequence action,
                                                           optional_register_id returned)
{
    match_instruction_case result;
    result.kind = match_instruction_case_kind_value;
    result.key_value = key_value;
    result.action = action;
    result.value = returned;
    return result;
}

match_instruction_case match_instruction_case_create_stateful_enum(match_instruction_case_stateful_enum stateful_enum,
                                                                   instruction_sequence action,
                                                                   optional_register_id returned)
{
    match_instruction_case result;
    result.kind = match_instruction_case_kind_stateful_enum;
    result.stateful_enum = stateful_enum;
    result.action = action;
    result.value = returned;
    return result;
}

match_instruction_case match_instruction_case_create_default(instruction_sequence action, optional_register_id returned)
{
    match_instruction_case result;
    result.kind = match_instruction_case_kind_default;
    result.key_value = ~(register_id)0;
    result.action = action;
    result.value = returned;
    return result;
}

void match_instruction_case_free(match_instruction_case freed)
{
    instruction_sequence_free(&freed.action);
}

bool match_instruction_case_equals(match_instruction_case const left, match_instruction_case const right)
{
    if (left.kind != right.kind)
    {
        return false;
    }
    switch (left.kind)
    {
    case match_instruction_case_kind_stateful_enum:
        if (left.stateful_enum.element != right.stateful_enum.element)
        {
            return false;
        }
        if (left.stateful_enum.where != right.stateful_enum.where)
        {
            return false;
        }
        break;

    case match_instruction_case_kind_value:
        if (left.key_value != right.key_value)
        {
            return false;
        }
        break;

    case match_instruction_case_kind_default:
        LPG_TO_DO();
    }
    return instruction_sequence_equals(&left.action, &right.action) &&
           optional_register_id_equals(left.value, right.value);
}

void match_instruction_case_swap(match_instruction_case *const first, match_instruction_case *const second)
{
    match_instruction_case const temp = *first;
    *first = *second;
    *second = temp;
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

instruction instruction_create_loop(loop_instruction loop)
{
    instruction result;
    result.type = instruction_loop;
    result.loop = loop;
    return result;
}

instruction instruction_create_break(register_id const into)
{
    instruction result;
    result.type = instruction_break;
    result.break_into = into;
    return result;
}

instruction instruction_create_literal(literal_instruction const content)
{
    instruction result;
    result.type = instruction_literal;
    result.literal = content;
    return result;
}

void instruction_free(instruction const *freed)
{
    switch (freed->type)
    {
    case instruction_current_function:
        current_function_instruction_free(freed->current_function);
        break;

    case instruction_call:
        call_instruction_free(&freed->call);
        break;

    case instruction_loop:
        loop_instruction_free(freed->loop);
        break;

    case instruction_new_array:
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
        deallocate(freed->tuple_.elements);
        deallocate(freed->tuple_.result_type.elements);
        break;

    case instruction_instantiate_struct:
        deallocate(freed->instantiate_struct.arguments);
        break;

    case instruction_match:
        for (size_t i = 0; i < freed->match.count; ++i)
        {
            match_instruction_case_free(freed->match.cases[i]);
        }
        if (freed->match.cases)
        {
            deallocate(freed->match.cases);
        }
        break;

    case instruction_lambda_with_captures:
        lambda_with_captures_instruction_free(freed->lambda_with_captures);
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
    case instruction_current_function:
    case instruction_new_array:
        LPG_TO_DO();

    case instruction_instantiate_struct:
        return instantiate_struct_instruction_equals(left.instantiate_struct, right.instantiate_struct);

    case instruction_get_method:
        LPG_TO_DO();

    case instruction_call:
        return call_instruction_equals(left.call, right.call);

    case instruction_return:
        return return_instruction_equals(left.return_, right.return_);

    case instruction_loop:
        return loop_instruction_equals(left.loop, right.loop);

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
            if (!match_instruction_case_equals(left.match.cases[i], right.match.cases[i]))
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

void add_instruction(LPG_NON_NULL(instruction_sequence *to), instruction const added)
{
    ASSUME(to);
    to->elements = reallocate_array(to->elements, (to->length + 1), sizeof(*to->elements));
    to->elements[to->length] = added;
    ++(to->length);
}
