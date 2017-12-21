#include "lpg_expression.h"
#include "lpg_for.h"
#include "lpg_allocate.h"
#include "lpg_assert.h"

void expression_deallocate(expression *this)
{
    expression_free(this);
    deallocate(this);
}

function_header_tree function_header_tree_create(parameter *parameters, size_t parameter_count, expression *return_type)
{
    function_header_tree const result = {parameters, parameter_count, return_type};
    return result;
}

void function_header_tree_free(function_header_tree value)
{
    LPG_FOR(size_t, i, value.parameter_count)
    {
        parameter_free(value.parameters + i);
    }
    if (value.parameters)
    {
        deallocate(value.parameters);
    }
    if (value.return_type != NULL)
    {
        expression_deallocate(value.return_type);
    }
}

bool function_header_tree_equals(function_header_tree const left, function_header_tree const right)
{
    if (left.parameter_count != right.parameter_count)
    {
        return false;
    }
    for (size_t i = 0; i < left.parameter_count; ++i)
    {
        if (!parameter_equals(left.parameters[i], right.parameters[i]))
        {
            return false;
        }
    }
    if (left.return_type && right.return_type)
    {
        return expression_equals(left.return_type, right.return_type);
    }
    return (left.return_type == right.return_type);
}

lambda lambda_create(function_header_tree header, expression *result)
{
    lambda const returning = {header, result};
    return returning;
}

void lambda_free(lambda const *this)
{
    function_header_tree_free(this->header);
    expression_deallocate(this->result);
}

bool lambda_equals(lambda const left, lambda const right)
{
    return function_header_tree_equals(left.header, right.header) && expression_equals(left.result, right.result);
}

call call_create(expression *callee, tuple arguments, source_location closing_parenthesis)
{
    call const result = {callee, arguments, closing_parenthesis};
    return result;
}

parameter parameter_create(identifier_expression name, expression *type)
{
    parameter const result = {name, type};
    return result;
}

void parameter_free(parameter *value)
{
    identifier_expression_free(&value->name);
    expression_deallocate(value->type);
}

bool parameter_equals(parameter const left, parameter const right)
{
    return identifier_expression_equals(left.name, right.name) && expression_equals(left.type, right.type);
}

access_structure access_structure_create(expression *object, identifier_expression member)
{
    access_structure const result = {object, member};
    return result;
}

match_case match_case_create(expression *key, expression *action)
{
    match_case const result = {key, action};
    return result;
}

void match_case_free(match_case *value)
{
    expression_deallocate(value->key);
    expression_deallocate(value->action);
}

bool match_case_equals(match_case const left, match_case const right)
{
    return expression_equals(left.key, right.key) && expression_equals(left.action, right.action);
}

match match_create(source_location begin, expression *input, match_case *cases, size_t number_of_cases)
{
    match const result = {begin, input, cases, number_of_cases};
    return result;
}

not not_expression_create(expression * value)
{
    not const result = {value};
    return result;
}

void not_free(LPG_NON_NULL(not const *value))
{
    expression_free(value->expr);
    deallocate(value->expr);
}

interface_expression_method interface_expression_method_create(identifier_expression name, function_header_tree header)
{
    interface_expression_method const result = {name, header};
    return result;
}

void interface_expression_method_free(interface_expression_method value)
{
    identifier_expression_free(&value.name);
    function_header_tree_free(value.header);
}

bool interface_expression_method_equals(interface_expression_method const left, interface_expression_method const right)
{
    return identifier_expression_equals(left.name, right.name) &&
           function_header_tree_equals(left.header, right.header);
}

interface_expression interface_expression_create(source_location source, interface_expression_method *methods,
                                                 size_t method_count)
{
    interface_expression const result = {source, methods, method_count};
    return result;
}

void interface_expression_free(interface_expression value)
{
    for (size_t i = 0; i < value.method_count; ++i)
    {
        interface_expression_method_free(value.methods[i]);
    }
    if (value.methods)
    {
        deallocate(value.methods);
    }
}

bool interface_expression_equals(interface_expression const left, interface_expression const right)
{
    if (left.method_count != right.method_count)
    {
        return false;
    }
    if (!source_location_equals(left.source, right.source))
    {
        return false;
    }
    for (size_t i = 0; i < left.method_count; ++i)
    {
        if (!interface_expression_method_equals(left.methods[i], right.methods[i]))
        {
            return false;
        }
    }
    return true;
}

struct_expression struct_expression_create(source_location source, struct_expression_element *elements,
                                           size_t element_count)
{
    struct_expression const result = {source, elements, element_count};
    return result;
}

void struct_expression_free(struct_expression const *const value)
{
    for (size_t i = 0; i < value->element_count; ++i)
    {
        struct_expression_element_free(&value->elements[i]);
    }
    if (value->elements)
    {
        deallocate(value->elements);
    }
}

bool struct_expression_equals(struct_expression const left, struct_expression const right)
{
    if (!source_location_equals(left.source, right.source))
    {
        return false;
    }
    if (left.element_count != right.element_count)
    {
        return false;
    }
    for (size_t i = 0; i < left.element_count; ++i)
    {
        if (!struct_expression_element_equals(left.elements[i], right.elements[i]))
        {
            return false;
        }
    }
    return true;
}

impl_expression_method impl_expression_method_create(identifier_expression name, function_header_tree header,
                                                     sequence body)
{
    impl_expression_method const result = {name, header, body};
    return result;
}

void impl_expression_method_free(impl_expression_method value)
{
    identifier_expression_free(&value.name);
    function_header_tree_free(value.header);
    sequence_free(&value.body);
}

bool impl_expression_method_equals(impl_expression_method const left, impl_expression_method const right)
{
    return identifier_expression_equals(left.name, right.name) &&
           function_header_tree_equals(left.header, right.header) && sequence_equals(left.body, right.body);
}

impl_expression impl_expression_create(expression *interface, expression *self, impl_expression_method *methods,
                                       size_t method_count)
{
    impl_expression const result = {interface, self, methods, method_count};
    return result;
}

void impl_expression_free(impl_expression value)
{
    expression_deallocate(value.interface);
    expression_deallocate(value.self);
    for (size_t i = 0; i < value.method_count; ++i)
    {
        impl_expression_method_free(value.methods[i]);
    }
    if (value.methods)
    {
        deallocate(value.methods);
    }
}

bool impl_expression_equals(impl_expression const left, impl_expression const right)
{
    if (!expression_equals(left.interface, right.interface) || !expression_equals(left.self, right.self) ||
        (left.method_count != right.method_count))
    {
        return false;
    }
    for (size_t i = 0; i < left.method_count; ++i)
    {
        if (!impl_expression_method_equals(left.methods[i], right.methods[i]))
        {
            return false;
        }
    }
    return true;
}

sequence sequence_create(expression *elements, size_t length)
{
    sequence const result = {elements, length};
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

declare declare_create(identifier_expression name, expression *optional_type, expression *initializer)
{
    ASSUME(initializer);
    declare const result = {name, optional_type, initializer};
    return result;
}

void declare_free(declare const *value)
{
    identifier_expression_free(&value->name);
    if (value->optional_type)
    {
        expression_deallocate(value->optional_type);
    }
    expression_deallocate(value->initializer);
}

tuple tuple_create(expression *elements, size_t length, source_location const opening_brace)
{
    tuple const result = {elements, length, opening_brace};
    return result;
}

bool tuple_equals(tuple const *left, tuple const *right)
{
    if (left->length != right->length)
    {
        return false;
    }
    for (size_t i = 0; i < left->length; ++i)
    {
        if (!expression_equals(left->elements + i, right->elements + i))
        {
            return false;
        }
    }
    return source_location_equals(left->opening_brace, right->opening_brace);
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

expression expression_from_not(not value)
{
    expression result;
    result.type = expression_type_not;
    result.not = value;
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

expression expression_from_impl(impl_expression value)
{
    expression result;
    result.type = expression_type_impl;
    result.impl = value;
    return result;
}

source_location expression_source_begin(expression const value)
{
    switch (value.type)
    {
    case expression_type_instantiate_struct:
        return expression_source_begin(*value.instantiate_struct.type);

    case expression_type_interface:
        return value.interface.source;

    case expression_type_call:
        return expression_source_begin(*value.call.callee);

    case expression_type_integer_literal:
        return value.integer_literal.source;

    case expression_type_access_structure:
        return expression_source_begin(*value.access_structure.object);

    case expression_type_match:
        return value.match.begin;

    case expression_type_string:
        return value.string.source;

    case expression_type_identifier:
        return value.identifier.source;

    case expression_type_lambda:
    case expression_type_return:
    case expression_type_loop:
    case expression_type_impl:
        LPG_TO_DO();

    case expression_type_break:
        return value.source;

    case expression_type_sequence:
        if (value.sequence.length > 0)
        {
            return expression_source_begin(value.sequence.elements[0]);
        }
        else
        {
            LPG_TO_DO();
        }

    case expression_type_declare:
        return value.declare.name.source;

    case expression_type_binary:
        return expression_source_begin(*value.binary.left);

    case expression_type_tuple:
        return value.tuple.opening_brace;

    case expression_type_not:
        return expression_source_begin(*value.not.expr);

    case expression_type_comment:
        return value.comment.source;

    case expression_type_struct:
        return value.struct_.source;
    }
    LPG_UNREACHABLE();
}

comment_expression comment_expression_create(unicode_string value, source_location source)
{
    comment_expression const result = {value, source};
    return result;
}

instantiate_struct_expression instantiate_struct_expression_create(expression *const type, tuple arguments)
{
    instantiate_struct_expression const result = {type, arguments};
    return result;
}

void instantiate_struct_expression_free(instantiate_struct_expression const freed)
{
    expression_deallocate(freed.type);
    tuple_free(&freed.arguments);
}

bool instantiate_struct_expression_equals(instantiate_struct_expression const left,
                                          instantiate_struct_expression const right)
{
    return expression_equals(left.type, right.type) && tuple_equals(&left.arguments, &right.arguments);
}

identifier_expression identifier_expression_create(unicode_string value, source_location source)
{
    identifier_expression const result = {value, source};
    return result;
}

void identifier_expression_free(identifier_expression const *value)
{
    unicode_string_free(&value->value);
}

bool identifier_expression_equals(identifier_expression const left, identifier_expression const right)
{
    return unicode_string_equals(left.value, right.value) && source_location_equals(left.source, right.source);
}

string_expression string_expression_create(unicode_string value, source_location source)
{
    string_expression const result = {value, source};
    return result;
}

void string_expression_free(string_expression const *value)
{
    unicode_string_free(&value->value);
}

void comment_expression_free(comment_expression const *value)
{
    unicode_string_free(&value->value);
}

integer_literal_expression integer_literal_expression_create(integer value, source_location source)
{
    integer_literal_expression const result = {value, source};
    return result;
}

bool integer_literal_expression_equals(integer_literal_expression const left, integer_literal_expression const right)
{
    return integer_equal(left.value, right.value) && source_location_equals(left.source, right.source);
}

struct_expression_element struct_expression_element_create(identifier_expression name, expression type)
{
    struct_expression_element const result = {name, type};
    return result;
}

void struct_expression_element_free(struct_expression_element const *const value)
{
    identifier_expression_free(&value->name);
    expression_free(&value->type);
}

bool struct_expression_element_equals(struct_expression_element const left, struct_expression_element const right)
{
    return identifier_expression_equals(left.name, right.name) && expression_equals(&left.type, &right.type);
}

expression expression_from_integer_literal(integer_literal_expression value)
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

expression expression_from_comment(comment_expression value)
{
    expression result;
    result.type = expression_type_comment;
    result.comment = value;
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

expression expression_from_break(source_location source)
{
    expression result;
    result.type = expression_type_break;
    result.source = source;
    return result;
}

expression expression_from_interface(interface_expression value)
{
    expression result;
    result.type = expression_type_interface;
    result.interface = value;
    return result;
}

expression expression_from_struct(struct_expression value)
{
    expression result;
    result.type = expression_type_struct;
    result.struct_ = value;
    return result;
}

expression expression_from_instantiate_struct(instantiate_struct_expression value)
{
    expression result;
    result.type = expression_type_instantiate_struct;
    result.instantiate_struct = value;
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
    case expression_type_instantiate_struct:
        instantiate_struct_expression_free(this->instantiate_struct);
        break;

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

    case expression_type_not:
        not_free(&this->not);
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

    case expression_type_comment:
        comment_expression_free(&this->comment);
        break;

    case expression_type_binary:
        binary_operator_expression_free(&this->binary);
        break;

    case expression_type_interface:
        interface_expression_free(this->interface);
        break;

    case expression_type_impl:
        impl_expression_free(this->impl);
        break;

    case expression_type_struct:
        struct_expression_free(&this->struct_);
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
    if (!identifier_expression_equals(left.name, right.name))
    {
        return false;
    }
    if (left.optional_type && right.optional_type)
    {
        return expression_equals(left.optional_type, right.optional_type);
    }
    return !left.optional_type && !right.optional_type;
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

bool comment_equals(comment_expression left, comment_expression right)
{
    bool const source_equals = source_location_equals(left.source, right.source);
    return unicode_string_equals(left.value, right.value) && source_equals;
}

bool expression_equals(expression const *left, expression const *right)
{
    if (left->type != right->type)
    {
        return 0;
    }
    switch (left->type)
    {
    case expression_type_instantiate_struct:
        return instantiate_struct_expression_equals(left->instantiate_struct, right->instantiate_struct);

    case expression_type_interface:
        return interface_expression_equals(left->interface, right->interface);

    case expression_type_lambda:
        return lambda_equals(left->lambda, right->lambda);

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
            if (!expression_equals(left->call.arguments.elements + i, right->call.arguments.elements + i))
            {
                return 0;
            }
        }
        return source_location_equals(left->call.closing_parenthesis, right->call.closing_parenthesis);

    case expression_type_integer_literal:
        return integer_literal_expression_equals(left->integer_literal, right->integer_literal);

    case expression_type_impl:
    case expression_type_access_structure:
    case expression_type_string:
        LPG_TO_DO();

    case expression_type_match:
        return match_equals(left->match, right->match);

    case expression_type_binary:
        return expression_equals(left->binary.left, right->binary.left) &&
               expression_equals(left->binary.right, right->binary.right) &&
               left->binary.comparator == right->binary.comparator;

    case expression_type_not:
        return expression_equals(left->not.expr, right->not.expr);

    case expression_type_identifier:
        return unicode_view_equals(unicode_view_from_string(left->identifier.value),
                                   unicode_view_from_string(right->identifier.value)) &&
               source_location_equals(left->identifier.source, right->identifier.source);

    case expression_type_return:
        return expression_equals(left->return_, right->return_);

    case expression_type_loop:
        return sequence_equals(left->loop_body, right->loop_body);

    case expression_type_break:
        return source_location_equals(left->source, right->source);

    case expression_type_sequence:
        return sequence_equals(left->sequence, right->sequence);

    case expression_type_declare:
        return declare_equals(left->declare, right->declare);

    case expression_type_tuple:
        return tuple_equals(&left->tuple, &right->tuple);

    case expression_type_comment:
        return comment_equals(left->comment, right->comment);

    case expression_type_struct:
        return struct_expression_equals(left->struct_, right->struct_);
    }
    LPG_UNREACHABLE();
}

binary_operator_expression binary_operator_expression_create(expression *left, expression *right,
                                                             binary_operator anOperator)
{
    binary_operator_expression result;
    result.left = left;
    result.right = right;
    result.comparator = anOperator;
    return result;
}

void binary_operator_expression_free(binary_operator_expression const *value)
{
    expression_free(value->left);
    deallocate(value->left);
    expression_free(value->right);
    deallocate(value->right);
}

expression expression_from_binary_operator(binary_operator_expression value)
{
    expression result;
    result.type = expression_type_binary;
    result.binary = value;
    return result;
}
