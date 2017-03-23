#include "lpg_expression.h"
#include "lpg_for.h"
#include "lpg_allocate.h"
#include "lpg_assert.h"

void expression_deallocate(expression *this)
{
    expression_free(this);
    deallocate(this);
}

lambda lambda_create(expression *parameter_type, expression *parameter_name,
                     expression *result)
{
    lambda returning = {parameter_type, parameter_name, result};
    return returning;
}

void lambda_free(lambda *this)
{
    expression_deallocate(this->parameter_type);
    expression_deallocate(this->parameter_name);
    expression_deallocate(this->result);
}

integer integer_create(uint64_t high, uint64_t low)
{
    integer result = {high, low};
    return result;
}

integer integer_shift_left(integer value, uint32_t bits)
{
    if (bits == 0)
    {
        return value;
    }
    if (bits == 64)
    {
        integer result = {value.low, 0};
        return result;
    }
    integer result = {((value.high << bits) | ((value.low >> (64u - bits)) &
                                               ((uint64_t)-1) >> (64u - bits))),
                      (value.low << bits)};
    return result;
}

unsigned integer_bit(integer value, uint32_t bit)
{
    if (bit < 64u)
    {
        return (value.low >> bit) & 1u;
    }
    return (value.high >> (bit - 64u)) & 1u;
}

void integer_set_bit(integer *target, uint32_t bit, unsigned value)
{
    if (bit < 64u)
    {
        target->low |= ((uint64_t)value << bit);
    }
    else
    {
        target->high |= ((uint64_t)value << (bit - 64u));
    }
}

unsigned integer_equal(integer left, integer right)
{
    return (left.high == right.high) && (left.low == right.low);
}

unsigned integer_less(integer left, integer right)
{
    if (left.high < right.high)
    {
        return 1;
    }
    if (left.high > right.high)
    {
        return 0;
    }
    return (left.low < right.low);
}

integer integer_subtract(integer minuend, integer subtrahend)
{
    integer result = {0, 0};
    result.low = (minuend.low - subtrahend.low);
    if (result.low > minuend.low)
    {
        --minuend.high;
    }
    result.high = (minuend.high - subtrahend.high);
    return result;
}

integer_division integer_divide(integer numerator, integer denominator)
{
    ASSERT(denominator.low || denominator.high);
    integer_division result = {{0, 0}, {0, 0}};
    for (unsigned i = 127; i < 128; --i)
    {
        result.remainder = integer_shift_left(result.remainder, 1);
        result.remainder.low |= integer_bit(numerator, i);
        if (!integer_less(result.remainder, denominator))
        {
            result.remainder = integer_subtract(result.remainder, denominator);
            integer_set_bit(&result.quotient, i, 1);
        }
    }
    return result;
}

call call_create(expression *callee, expression *arguments,
                 size_t number_of_arguments)
{
    call result = {callee, arguments, number_of_arguments};
    return result;
}

void function_free(function *this)
{
    expression_deallocate(this->result);
    expression_deallocate(this->parameter);
}

void add_member_free(add_member *this)
{
    expression_deallocate(this->base);
    expression_deallocate(this->name);
    expression_deallocate(this->type);
}

expression expression_from_builtin(builtin value)
{
    expression result;
    result.type = expression_type_builtin;
    result.builtin = value;
    return result;
}

expression expression_from_integer_literal(integer value)
{
    expression result;
    result.type = expression_type_integer_literal;
    result.integer_literal = value;
    return result;
}

void call_free(call *this)
{
    expression_deallocate(this->callee);
    LPG_FOR(size_t, i, this->number_of_arguments)
    {
        expression_free(this->arguments + i);
    }
    deallocate(this->arguments);
}

void fill_structure_free(fill_structure *this)
{
    LPG_FOR(size_t, i, this->number_of_members)
    {
        expression_free(this->members + i);
    }
    deallocate(this->members);
}

void access_structure_free(access_structure *this)
{
    expression_deallocate(this->object);
    unicode_string_free(&this->member);
}

void add_to_variant_free(add_to_variant *this)
{
    expression_deallocate(this->base);
    expression_deallocate(this->new_type);
}

void match_free(match *this)
{
    expression_deallocate(this->input);
    LPG_FOR(size_t, i, this->number_of_cases)
    {
        expression_free(this->cases[i].key);
        expression_free(this->cases[i].action);
    }
    deallocate(this->cases);
}

void sequence_free(sequence *this)
{
    LPG_FOR(size_t, i, this->number_of_elements)
    {
        expression_free(this->elements + i);
    }
    deallocate(this->elements);
}

void assignment_free(assignment *this)
{
    expression_deallocate(this->variable);
    expression_deallocate(this->new_value);
}

expression expression_from_lambda(lambda lambda)
{
    expression result;
    result.type = expression_type_lambda;
    result.lambda = lambda;
    return result;
}

expression expression_from_unicode_string(unicode_string value)
{
    expression result;
    result.type = expression_type_string;
    result.string = value;
    return result;
}

expression expression_from_call(call value)
{
    expression result;
    result.type = expression_type_call;
    result.call = value;
    return result;
}

expression expression_from_identifier(unicode_string identifier)
{
    expression result;
    result.type = expression_type_identifier;
    result.identifier = identifier;
    return result;
}

expression *expression_allocate(expression value)
{
    expression *result = allocate(sizeof(*result));
    *result = value;
    return result;
}

void expression_free(expression *this)
{
    switch (this->type)
    {
    case expression_type_lambda:
        lambda_free(&this->lambda);
        break;

    case expression_type_builtin:
        break;

    case expression_type_call:
        call_free(&this->call);
        break;

    case expression_type_local:
        expression_free(this->local);
        break;

    case expression_type_integer_literal:
        break;

    case expression_type_integer_range:
        break;

    case expression_type_function:
        function_free(&this->function);
        break;

    case expression_type_add_member:
        add_member_free(&this->add_member);
        break;

    case expression_type_fill_structure:
        fill_structure_free(&this->fill_structure);
        break;

    case expression_type_access_structure:
        access_structure_free(&this->access_structure);
        break;

    case expression_type_add_to_variant:
        add_to_variant_free(&this->add_to_variant);
        break;

    case expression_type_match:
        match_free(&this->match);
        break;

    case expression_type_sequence:
        sequence_free(&this->sequence);
        break;

    case expression_type_assignment:
        assignment_free(&this->assignment);
        break;

    case expression_type_string:
        unicode_string_free(&this->string);
        break;

    case expression_type_identifier:
        unicode_string_free(&this->identifier);
        break;
    }
}
