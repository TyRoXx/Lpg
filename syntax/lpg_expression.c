#include "lpg_expression.h"
#include "lpg_for.h"
#include "lpg_allocate.h"
#include "lpg_assert.h"
#include "lpg_unicode_view.h"
#include <stdbool.h>

void expression_deallocate(expression *this)
{
    expression_free(this);
    deallocate(this);
}

parameter parameter_create(expression *name, expression *type)
{
    parameter result = {name, type};
    return result;
}

void parameter_free(parameter *value)
{
    expression_deallocate(value->name);
    expression_deallocate(value->type);
}

lambda lambda_create(parameter *parameters, size_t parameter_count,
                     expression *result)
{
    lambda returning = {parameters, parameter_count, result};
    return returning;
}

void lambda_free(lambda const *this)
{
    LPG_FOR(size_t, i, this->parameter_count)
    {
        parameter_free(this->parameters + i);
    }
    if (this->parameters)
    {
        deallocate(this->parameters);
    }
    expression_deallocate(this->result);
}

call call_create(expression *callee, tuple arguments,
                 source_location closing_parenthesis)
{
    call result = {callee, arguments, closing_parenthesis};
    return result;
}

access_structure access_structure_create(expression *object,
                                         identifier_expression member)
{
    access_structure result = {object, member};
    return result;
}

match_case match_case_create(expression *key, expression *action)
{
    match_case result = {key, action};
    return result;
}

void match_case_free(match_case *value)
{
    expression_deallocate(value->key);
    expression_deallocate(value->action);
}

bool match_case_equals(match_case const left, match_case const right)
{
    return expression_equals(left.key, right.key) &&
           expression_equals(left.action, right.action);
}

match match_create(expression *input, match_case *cases, size_t number_of_cases)
{
    match result = {input, cases, number_of_cases};
    return result;
}

assign assign_create(expression *left, expression *right)
{
    assign result = {left, right};
    return result;
}

sequence sequence_create(expression *elements, size_t length)
{
    sequence result = {elements, length};
    return result;
}

void sequence_free(sequence const *value)
{
    LPG_FOR(size_t, i, value->length)
    {
        expression_free(value->elements + i);
    }
    if (value->elements)
    {
        deallocate(value->elements);
    }
}

declare declare_create(expression *name, expression *type,
                       expression *initializer)
{
    ASSUME(name);
    ASSUME(type);
    ASSUME(initializer);
    declare result = {name, type, initializer};
    return result;
}

void declare_free(declare const *value)
{
    expression_deallocate(value->name);
    expression_deallocate(value->type);
    expression_deallocate(value->initializer);
}

tuple tuple_create(expression *elements, size_t length)
{
    tuple result = {elements, length};
    return result;
}

void tuple_free(tuple const *value)
{
    LPG_FOR(size_t, i, value->length)
    {
        expression_free(value->elements + i);
    }
    if (value->length > 0)
    {
        deallocate(value->elements);
    }
}

expression expression_from_assign(assign value)
{
    expression result;
    result.type = expression_type_assign;
    result.assign = value;
    return result;
}

expression expression_from_return(expression *value)
{
    expression result;
    result.type = expression_type_return;
    result.return_ = value;
    return result;
}

expression expression_from_loop(sequence body)
{
    expression result;
    result.type = expression_type_loop;
    result.loop_body = body;
    return result;
}

expression expression_from_break(void)
{
    expression result;
    result.type = expression_type_break;
    return result;
}

expression expression_from_sequence(sequence value)
{
    expression result;
    result.type = expression_type_sequence;
    result.sequence = value;
    return result;
}

expression expression_from_access_structure(access_structure value)
{
    expression result;
    result.type = expression_type_access_structure;
    result.access_structure = value;
    return result;
}

expression expression_from_declare(declare value)
{
    expression result;
    result.type = expression_type_declare;
    result.declare = value;
    return result;
}

expression expression_from_match(match value)
{
    expression result;
    result.type = expression_type_match;
    result.match = value;
    return result;
}

source_location expression_source_begin(expression const value)
{
    switch (value.type)
    {
    case expression_type_lambda:
        LPG_TO_DO();
    case expression_type_call:
        LPG_TO_DO();
    case expression_type_integer_literal:
        LPG_TO_DO();

    case expression_type_access_structure:
        return expression_source_begin(*value.access_structure.object);

    case expression_type_match:
        LPG_TO_DO();

    case expression_type_string:
        return value.string.source;

    case expression_type_identifier:
        return value.identifier.source;

    case expression_type_assign:
        LPG_TO_DO();
    case expression_type_return:
        LPG_TO_DO();
    case expression_type_loop:
        LPG_TO_DO();
    case expression_type_break:
        LPG_TO_DO();
    case expression_type_sequence:
        LPG_TO_DO();
    case expression_type_declare:
        LPG_TO_DO();
    case expression_type_tuple:
        LPG_TO_DO();
    }
    UNREACHABLE();
}

identifier_expression identifier_expression_create(unicode_string value,
                                                   source_location source)
{
    identifier_expression result = {value, source};
    return result;
}

void identifier_expression_free(identifier_expression const *value)
{
    unicode_string_free(&value->value);
}

string_expression string_expression_create(unicode_string value,
                                           source_location source)
{
    string_expression result = {value, source};
    return result;
}

void string_expression_free(string_expression const *value)
{
    unicode_string_free(&value->value);
}

expression expression_from_integer_literal(integer value)
{
    expression result;
    result.type = expression_type_integer_literal;
    result.integer_literal = value;
    return result;
}

void call_free(call const *this)
{
    expression_deallocate(this->callee);
    tuple_free(&this->arguments);
}

void access_structure_free(access_structure const *this)
{
    expression_deallocate(this->object);
    identifier_expression_free(&this->member);
}

void match_free(match const *this)
{
    expression_deallocate(this->input);
    LPG_FOR(size_t, i, this->number_of_cases)
    {
        match_case_free(this->cases + i);
    }
    if (this->cases)
    {
        deallocate(this->cases);
    }
}

expression expression_from_lambda(lambda lambda)
{
    expression result;
    result.type = expression_type_lambda;
    result.lambda = lambda;
    return result;
}

expression expression_from_string(string_expression value)
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

expression expression_from_identifier(identifier_expression identifier)
{
    expression result;
    result.type = expression_type_identifier;
    result.identifier = identifier;
    return result;
}

expression expression_from_tuple(tuple value)
{
    expression result;
    result.type = expression_type_tuple;
    result.tuple = value;
    return result;
}

expression *expression_allocate(expression value)
{
    expression *result = allocate(sizeof(*result));
    *result = value;
    return result;
}

void expression_free(expression const *this)
{
    switch (this->type)
    {
    case expression_type_lambda:
        lambda_free(&this->lambda);
        break;

    case expression_type_call:
        call_free(&this->call);
        break;

    case expression_type_integer_literal:
        break;

    case expression_type_access_structure:
        access_structure_free(&this->access_structure);
        break;

    case expression_type_match:
        match_free(&this->match);
        break;

    case expression_type_string:
        string_expression_free(&this->string);
        break;

    case expression_type_identifier:
        identifier_expression_free(&this->identifier);
        break;

    case expression_type_assign:
        expression_deallocate(this->assign.left);
        expression_deallocate(this->assign.right);
        break;

    case expression_type_return:
        expression_deallocate(this->return_);
        break;

    case expression_type_loop:
        sequence_free(&this->loop_body);
        break;

    case expression_type_break:
        break;

    case expression_type_sequence:
        sequence_free(&this->sequence);
        break;

    case expression_type_declare:
        declare_free(&this->declare);
        break;

    case expression_type_tuple:
        tuple_free(&this->tuple);
        break;
    }
}

bool sequence_equals(sequence const left, sequence const right)
{
    if (left.length != right.length)
    {
        return 0;
    }
    LPG_FOR(size_t, i, left.length)
    {
        if (!expression_equals(left.elements + i, right.elements + i))
        {
            return 0;
        }
    }
    return 1;
}

bool declare_equals(declare const left, declare const right)
{
    return expression_equals(left.name, right.name) &&
           expression_equals(left.type, right.type);
}

bool assign_equals(assign const left, assign const right)
{
    return expression_equals(left.left, right.left) &&
           expression_equals(left.right, right.right);
}

bool match_equals(match const left, match const right)
{
    if (!expression_equals(left.input, right.input))
    {
        return false;
    }
    if (left.number_of_cases != right.number_of_cases)
    {
        return false;
    }
    LPG_FOR(size_t, i, left.number_of_cases)
    {
        if (!match_case_equals(left.cases[i], right.cases[i]))
        {
            return false;
        }
    }
    return true;
}

bool expression_equals(expression const *left, expression const *right)
{
    if (left->type != right->type)
    {
        return 0;
    }
    switch (left->type)
    {
    case expression_type_lambda:
        LPG_TO_DO();

    case expression_type_call:
        if (!expression_equals(left->call.callee, right->call.callee))
        {
            return 0;
        }
        if (left->call.arguments.length != right->call.arguments.length)
        {
            return 0;
        }
        LPG_FOR(size_t, i, left->call.arguments.length)
        {
            if (!expression_equals(left->call.arguments.elements + i,
                                   right->call.arguments.elements + i))
            {
                return 0;
            }
        }
        return source_location_equals(
            left->call.closing_parenthesis, right->call.closing_parenthesis);

    case expression_type_integer_literal:
        return integer_equal(left->integer_literal, right->integer_literal);

    case expression_type_access_structure:
        LPG_TO_DO();

    case expression_type_match:
        return match_equals(left->match, right->match);

    case expression_type_string:
        LPG_TO_DO();

    case expression_type_identifier:
        return unicode_view_equals(
                   unicode_view_from_string(left->identifier.value),
                   unicode_view_from_string(right->identifier.value)) &&
               source_location_equals(
                   left->identifier.source, right->identifier.source);

    case expression_type_assign:
        return assign_equals(left->assign, right->assign);

    case expression_type_return:
        return expression_equals(left->return_, right->return_);

    case expression_type_loop:
        return sequence_equals(left->loop_body, right->loop_body);

    case expression_type_break:
        return 1;

    case expression_type_sequence:
        return sequence_equals(left->sequence, right->sequence);

    case expression_type_declare:
        return declare_equals(left->declare, right->declare);

    case expression_type_tuple:
        LPG_TO_DO();
    }
    UNREACHABLE();
}
