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

call call_create(expression *callee, expression *arguments,
                 size_t number_of_arguments)
{
    call result = {callee, arguments, number_of_arguments};
    return result;
}

void add_member_free(add_member *this)
{
    expression_deallocate(this->base);
    expression_deallocate(this->name);
    expression_deallocate(this->type);
}

access_structure access_structure_create(expression *object, expression *member)
{
    access_structure result = {object, member};
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

declare declare_create(expression *name, expression *type,
                       expression *optional_initializer)
{
    declare result = {name, type, optional_initializer};
    return result;
}

void declare_free(declare *value)
{
    expression_deallocate(value->name);
    expression_deallocate(value->type);
    if (value->optional_initializer)
    {
        expression_deallocate(value->optional_initializer);
    }
}

tuple tuple_create(expression *elements, size_t length)
{
    tuple result = {elements, length};
    return result;
}

void tuple_free(tuple *value)
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

expression expression_from_loop(expression *body)
{
    expression result;
    result.type = expression_type_loop;
    result.loop_body = body;
    return result;
}

expression expression_from_break()
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
    expression_deallocate(this->member);
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

expression expression_from_make_identifier(expression *value)
{
    expression result;
    result.type = expression_type_make_identifier;
    result.make_identifier = value;
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

void expression_free(expression *this)
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

    case expression_type_integer_range:
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

    case expression_type_string:
        unicode_string_free(&this->string);
        break;

    case expression_type_identifier:
        unicode_string_free(&this->identifier);
        break;

    case expression_type_make_identifier:
        expression_deallocate(this->make_identifier);
        break;

    case expression_type_assign:
        expression_deallocate(this->assign.left);
        expression_deallocate(this->assign.right);
        break;

    case expression_type_return:
        expression_deallocate(this->return_);
        break;

    case expression_type_loop:
        expression_deallocate(this->loop_body);
        break;

    case expression_type_break:
        break;

    case expression_type_sequence:
        LPG_FOR(size_t, i, this->sequence.length)
        {
            expression_free(this->sequence.elements + i);
        }
        deallocate(this->sequence.elements);
        break;

    case expression_type_declare:
        declare_free(&this->declare);
        break;

    case expression_type_tuple:
        tuple_free(&this->tuple);
        break;
    }
}
