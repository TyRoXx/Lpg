#include "lpg_expression.h"
#include "lpg_allocate.h"
#include "lpg_assert.h"
#include "lpg_for.h"

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

function_header_tree function_header_tree_clone(function_header_tree const original)
{
    parameter *const parameters = allocate_array(original.parameter_count, sizeof(*parameters));
    for (size_t i = 0; i < original.parameter_count; ++i)
    {
        parameters[i] = parameter_clone(original.parameters[i]);
    }
    expression *const return_type =
        original.return_type ? expression_allocate(expression_clone(*original.return_type)) : NULL;
    function_header_tree const result = function_header_tree_create(parameters, original.parameter_count, return_type);
    ASSUME(function_header_tree_equals(original, result));
    return result;
}

lambda lambda_create(generic_parameter_list generic_parameters, function_header_tree header, expression *result,
                     source_location source)
{
    lambda const returning = {generic_parameters, header, result, source};
    return returning;
}

void lambda_free(lambda const *this)
{
    generic_parameter_list_free(this->generic_parameters);
    function_header_tree_free(this->header);
    expression_deallocate(this->result);
}

bool lambda_equals(lambda const left, lambda const right)
{
    return generic_parameter_list_equals(left.generic_parameters, right.generic_parameters) &&
           function_header_tree_equals(left.header, right.header) && expression_equals(left.result, right.result) &&
           source_location_equals(left.source, right.source);
}

lambda lambda_clone(lambda const original)
{
    return lambda_create(
        generic_parameter_list_clone(original.generic_parameters), function_header_tree_clone(original.header),
        original.result ? expression_allocate(expression_clone(*original.result)) : NULL, original.source);
}

tuple tuple_clone(tuple const original)
{
    expression *const elements = allocate_array(original.length, sizeof(*elements));
    for (size_t i = 0; i < original.length; ++i)
    {
        elements[i] = expression_clone(original.elements[i]);
    }
    return tuple_create(elements, original.length, original.opening_brace);
}

call call_create(expression *callee, tuple arguments, source_location closing_parenthesis)
{
    call const result = {callee, arguments, closing_parenthesis};
    return result;
}

call call_clone(call const original)
{
    return call_create(expression_allocate(expression_clone(*original.callee)), tuple_clone(original.arguments),
                       original.closing_parenthesis);
}

comment_expression comment_expression_clone(comment_expression const original)
{
    return comment_expression_create(unicode_view_copy(unicode_view_from_string(original.value)), original.source);
}

identifier_expression identifier_expression_clone(identifier_expression const original)
{
    return identifier_expression_create(unicode_view_copy(unicode_view_from_string(original.value)), original.source);
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

parameter parameter_clone(parameter const original)
{
    ASSUME(original.type);
    return parameter_create(
        identifier_expression_clone(original.name), expression_allocate(expression_clone(*original.type)));
}

access_structure access_structure_create(expression *object, identifier_expression member)
{
    access_structure const result = {object, member};
    return result;
}

access_structure access_structure_clone(access_structure const original)
{
    ASSUME(original.object);
    return access_structure_create(
        expression_allocate(expression_clone(*original.object)), identifier_expression_clone(original.member));
}

bool access_structure_equals(access_structure const left, access_structure const right)
{
    return expression_equals(left.object, right.object) && identifier_expression_equals(left.member, right.member);
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

match_case match_case_clone(match_case const original)
{
    return match_case_create(
        expression_allocate(expression_clone(*original.key)), expression_allocate(expression_clone(*original.action)));
}

match match_create(source_location begin, expression *input, match_case *cases, size_t number_of_cases)
{
    match const result = {begin, input, cases, number_of_cases};
    return result;
}

match match_clone(match const original)
{
    match_case *const cases = allocate_array(original.number_of_cases, sizeof(*cases));
    for (size_t i = 0; i < original.number_of_cases; ++i)
    {
        cases[i] = match_case_clone(original.cases[i]);
    }
    return match_create(
        original.begin, expression_allocate(expression_clone(*original.input)), cases, original.number_of_cases);
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

not not_clone(not const original)
{
    return not_expression_create(expression_allocate(expression_clone(*original.expr)));
}

declare declare_clone(declare const original)
{
    return declare_create(
        identifier_expression_clone(original.name),
        original.optional_type ? expression_allocate(expression_clone(*original.optional_type)) : NULL,
        expression_allocate(expression_clone(*original.initializer)));
}

generic_parameter_list generic_parameter_list_create(unicode_string *names, size_t count)
{
    generic_parameter_list const result = {names, count};
    return result;
}

void generic_parameter_list_free(generic_parameter_list const freed)
{
    for (size_t i = 0; i < freed.count; ++i)
    {
        unicode_string_free(&freed.names[i]);
    }
    if (freed.names)
    {
        deallocate(freed.names);
    }
}

bool generic_parameter_list_equals(generic_parameter_list const left, generic_parameter_list const right)
{
    if (left.count != right.count)
    {
        return false;
    }
    for (size_t i = 0; i < left.count; ++i)
    {
        if (!unicode_string_equals(left.names[i], right.names[i]))
        {
            return false;
        }
    }
    return true;
}

generic_parameter_list generic_parameter_list_clone(generic_parameter_list const original)
{
    unicode_string *const names = allocate_array(original.count, sizeof(*names));
    for (size_t i = 0; i < original.count; ++i)
    {
        names[i] = unicode_view_copy(unicode_view_from_string(original.names[i]));
    }
    return generic_parameter_list_create(names, original.count);
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

interface_expression_method interface_expression_method_clone(interface_expression_method const original)
{
    return interface_expression_method_create(
        identifier_expression_clone(original.name), function_header_tree_clone(original.header));
}

interface_expression interface_expression_create(generic_parameter_list parameters, source_location source,
                                                 interface_expression_method *methods, size_t method_count)
{
    interface_expression const result = {parameters, source, methods, method_count};
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
    generic_parameter_list_free(value.parameters);
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

interface_expression interface_expression_clone(interface_expression const original)
{
    interface_expression_method *const methods = allocate_array(original.method_count, sizeof(*methods));
    for (size_t i = 0; i < original.method_count; ++i)
    {
        methods[i] = interface_expression_method_clone(original.methods[i]);
    }
    interface_expression const result = interface_expression_create(
        generic_parameter_list_clone(original.parameters), original.source, methods, original.method_count);
    return result;
}

struct_expression struct_expression_create(generic_parameter_list generic_parameters, source_location source,
                                           struct_expression_element *elements, size_t element_count)
{
    struct_expression const result = {generic_parameters, source, elements, element_count};
    return result;
}

void struct_expression_free(struct_expression const *const value)
{
    generic_parameter_list_free(value->generic_parameters);
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
    if (!generic_parameter_list_equals(left.generic_parameters, right.generic_parameters))
    {
        return false;
    }
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

struct_expression struct_expression_clone(struct_expression const original)
{
    struct_expression_element *const elements = allocate_array(original.element_count, sizeof(*elements));
    for (size_t i = 0; i < original.element_count; ++i)
    {
        elements[i] = struct_expression_element_clone(original.elements[i]);
    }
    return struct_expression_create(
        generic_parameter_list_clone(original.generic_parameters), original.source, elements, original.element_count);
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

impl_expression_method impl_expression_method_clone(impl_expression_method const original)
{
    return impl_expression_method_create(identifier_expression_clone(original.name),
                                         function_header_tree_clone(original.header), sequence_clone(original.body));
}

impl_expression impl_expression_create(generic_parameter_list generic_parameters, expression *interface_,
                                       expression *self, impl_expression_method *methods, size_t method_count)
{
    impl_expression const result = {generic_parameters, interface_, self, methods, method_count};
    return result;
}

void impl_expression_free(impl_expression value)
{
    generic_parameter_list_free(value.generic_parameters);
    expression_deallocate(value.interface_);
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
    if (!generic_parameter_list_equals(left.generic_parameters, right.generic_parameters))
    {
        return false;
    }
    if (!expression_equals(left.interface_, right.interface_) || !expression_equals(left.self, right.self) ||
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

impl_expression impl_expression_clone(impl_expression const original)
{
    impl_expression_method *const methods = allocate_array(original.method_count, sizeof(*methods));
    for (size_t i = 0; i < original.method_count; ++i)
    {
        methods[i] = impl_expression_method_clone(original.methods[i]);
    }
    return impl_expression_create(generic_parameter_list_clone(original.generic_parameters),
                                  expression_allocate(expression_clone(*original.interface_)),
                                  expression_allocate(expression_clone(*original.self)), methods,
                                  original.method_count);
}

placeholder_expression placeholder_expression_create(source_location where, unicode_string name)
{
    placeholder_expression const result = {where, name};
    return result;
}

void placeholder_expression_free(placeholder_expression const freed)
{
    unicode_string_free(&freed.name);
}

bool placeholder_expression_equals(placeholder_expression const left, placeholder_expression const right)
{
    return source_location_equals(left.where, right.where) && unicode_string_equals(left.name, right.name);
}

placeholder_expression placeholder_expression_clone(placeholder_expression const original)
{
    return placeholder_expression_create(original.where, unicode_view_copy(unicode_view_from_string(original.name)));
}

new_array_expression new_array_expression_create(expression *element)
{
    new_array_expression const result = {element};
    return result;
}

void new_array_expression_free(new_array_expression const freed)
{
    expression_deallocate(freed.element);
}

bool new_array_expression_equals(new_array_expression const left, new_array_expression const right)
{
    return expression_equals(left.element, right.element);
}

new_array_expression new_array_expression_clone(new_array_expression const original)
{
    ASSUME(original.element);
    return new_array_expression_create(expression_allocate(expression_clone(*original.element)));
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
    if (value->elements)
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
    case expression_type_new_array:
        return expression_source_begin(*value.new_array.element);

    case expression_type_import:
        LPG_TO_DO();

    case expression_type_generic_instantiation:
        return expression_source_begin(*value.generic_instantiation.generic);

    case expression_type_type_of:
        return value.type_of.begin;

    case expression_type_enum:
        return value.enum_.begin;

    case expression_type_instantiate_struct:
        return expression_source_begin(*value.instantiate_struct.type);

    case expression_type_interface:
        return value.interface_.source;

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
        return value.lambda.source;

    case expression_type_return:
        return expression_source_begin(*value.return_);

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

    case expression_type_placeholder:
        return value.placeholder.where;
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

instantiate_struct_expression instantiate_struct_expression_clone(instantiate_struct_expression const original)
{
    return instantiate_struct_expression_create(
        expression_allocate(expression_clone(*original.type)), tuple_clone(original.arguments));
}

enum_expression_element enum_expression_element_create(unicode_string name, expression *state)
{
    enum_expression_element const result = {name, state};
    return result;
}

void enum_expression_element_free(enum_expression_element const freed)
{
    unicode_string_free(&freed.name);
    if (freed.state)
    {
        expression_deallocate(freed.state);
    }
}

bool enum_expression_element_equals(enum_expression_element const left, enum_expression_element const right)
{
    if (!unicode_string_equals(left.name, right.name))
    {
        return false;
    }
    if (left.state)
    {
        if (right.state)
        {
            return expression_equals(left.state, right.state);
        }
        return false;
    }
    return !right.state;
}

enum_expression_element enum_expression_element_clone(enum_expression_element const original)
{
    return enum_expression_element_create(
        unicode_view_copy(unicode_view_from_string(original.name)),
        original.state ? expression_allocate(expression_clone(*original.state)) : NULL);
}

enum_expression enum_expression_create(source_location const begin, generic_parameter_list parameters,
                                       enum_expression_element *const elements, enum_element_id const element_count)
{
    enum_expression const result = {begin, parameters, elements, element_count};
    return result;
}

void enum_expression_free(enum_expression const freed)
{
    for (size_t i = 0; i < freed.element_count; ++i)
    {
        enum_expression_element_free(freed.elements[i]);
    }
    if (freed.elements)
    {
        deallocate(freed.elements);
    }
    generic_parameter_list_free(freed.parameters);
}

bool enum_expression_equals(enum_expression const left, enum_expression const right)
{
    if (!generic_parameter_list_equals(left.parameters, right.parameters))
    {
        return false;
    }
    if (!source_location_equals(left.begin, right.begin))
    {
        return false;
    }
    if (left.element_count != right.element_count)
    {
        return false;
    }
    for (size_t i = 0; i < left.element_count; ++i)
    {
        if (!enum_expression_element_equals(left.elements[i], right.elements[i]))
        {
            return false;
        }
    }
    return true;
}

enum_expression enum_expression_clone(enum_expression const original)
{
    enum_expression_element *const elements = allocate_array(original.element_count, sizeof(*elements));
    for (size_t i = 0; i < original.element_count; ++i)
    {
        elements[i] = enum_expression_element_clone(original.elements[i]);
    }
    return enum_expression_create(
        original.begin, generic_parameter_list_clone(original.parameters), elements, original.element_count);
}

generic_instantiation_expression generic_instantiation_expression_create(expression *generic, expression *arguments,
                                                                         size_t count)
{
    generic_instantiation_expression const result = {generic, arguments, count};
    return result;
}

void generic_instantiation_expression_free(generic_instantiation_expression const freed)
{
    expression_deallocate(freed.generic);
    for (size_t i = 0; i < freed.count; ++i)
    {
        expression_free(freed.arguments + i);
    }
    if (freed.arguments)
    {
        deallocate(freed.arguments);
    }
}

bool generic_instantiation_expression_equals(generic_instantiation_expression const left,
                                             generic_instantiation_expression const right)
{
    if (!expression_equals(left.generic, right.generic))
    {
        return false;
    }
    if (left.count != right.count)
    {
        return false;
    }
    for (size_t i = 0; i < left.count; ++i)
    {
        if (!expression_equals(&left.arguments[i], &right.arguments[i]))
        {
            return false;
        }
    }
    return true;
}

generic_instantiation_expression generic_instantiation_expression_clone(generic_instantiation_expression const original)
{
    expression *const arguments = allocate_array(original.count, sizeof(*arguments));
    for (size_t i = 0; i < original.count; ++i)
    {
        arguments[i] = expression_clone(original.arguments[i]);
    }
    return generic_instantiation_expression_create(
        expression_allocate(expression_clone(*original.generic)), arguments, original.count);
}

type_of_expression type_of_expression_create(source_location begin, expression *target)
{
    type_of_expression const result = {begin, target};
    return result;
}

void type_of_expression_free(type_of_expression const freed)
{
    expression_deallocate(freed.target);
}

bool type_of_expression_equals(type_of_expression const left, type_of_expression const right)
{
    return source_location_equals(left.begin, right.begin) && expression_equals(left.target, right.target);
}

type_of_expression type_of_expression_clone(type_of_expression const original)
{
    ASSUME(original.target);
    return type_of_expression_create(original.begin, expression_allocate(expression_clone(*original.target)));
}

import_expression import_expression_create(source_location begin, identifier_expression name)
{
    import_expression const result = {begin, name};
    return result;
}

void import_expression_free(import_expression const freed)
{
    identifier_expression_free(&freed.name);
}

bool import_expression_equals(import_expression const left, import_expression const right)
{
    return source_location_equals(left.begin, right.begin) && identifier_expression_equals(left.name, right.name);
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

string_expression string_expression_clone(string_expression const original)
{
    return string_expression_create(unicode_view_copy(unicode_view_from_string(original.value)), original.source);
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

integer_literal_expression integer_literal_expression_clone(integer_literal_expression const original)
{
    return integer_literal_expression_create(original.value, original.source);
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

struct_expression_element struct_expression_element_clone(struct_expression_element const original)
{
    return struct_expression_element_create(
        identifier_expression_clone(original.name), expression_clone(original.type));
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

expression expression_from_lambda(lambda content)
{
    expression result;
    result.type = expression_type_lambda;
    result.lambda = content;
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
    result.interface_ = value;
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

expression expression_from_enum(enum_expression const value)
{
    expression result;
    result.type = expression_type_enum;
    result.enum_ = value;
    return result;
}

expression expression_from_placeholder(placeholder_expression const value)
{
    expression result;
    result.type = expression_type_placeholder;
    result.placeholder = value;
    return result;
}

expression expression_from_generic_instantiation(generic_instantiation_expression const value)
{
    expression result;
    result.type = expression_type_generic_instantiation;
    result.generic_instantiation = value;
    return result;
}

expression expression_from_type_of(type_of_expression const value)
{
    expression result;
    result.type = expression_type_type_of;
    result.type_of = value;
    return result;
}

expression expression_from_import(import_expression const value)
{
    expression result;
    result.type = expression_type_import;
    result.import = value;
    return result;
}

expression expression_from_new_array(new_array_expression const content)
{
    expression result;
    result.type = expression_type_new_array;
    result.new_array = content;
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
    case expression_type_new_array:
        new_array_expression_free(this->new_array);
        break;

    case expression_type_import:
        import_expression_free(this->import);
        break;

    case expression_type_generic_instantiation:
        generic_instantiation_expression_free(this->generic_instantiation);
        break;

    case expression_type_type_of:
        type_of_expression_free(this->type_of);
        break;

    case expression_type_enum:
        enum_expression_free(this->enum_);
        break;

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

    case expression_type_placeholder:
        placeholder_expression_free(this->placeholder);
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
        interface_expression_free(this->interface_);
        break;

    case expression_type_impl:
        impl_expression_free(this->impl);
        break;

    case expression_type_struct:
        struct_expression_free(&this->struct_);
        break;
    }
}

expression expression_clone(expression const original)
{
    switch (original.type)
    {
    case expression_type_new_array:
        return expression_from_new_array(new_array_expression_clone(original.new_array));

    case expression_type_identifier:
        return expression_from_identifier(identifier_expression_clone(original.identifier));

    case expression_type_access_structure:
        return expression_from_access_structure(access_structure_clone(original.access_structure));

    case expression_type_generic_instantiation:
        return expression_from_generic_instantiation(
            generic_instantiation_expression_clone(original.generic_instantiation));

    case expression_type_call:
        return expression_from_call(call_clone(original.call));

    case expression_type_lambda:
        return expression_from_lambda(lambda_clone(original.lambda));

    case expression_type_sequence:
        return expression_from_sequence(sequence_clone(original.sequence));

    case expression_type_declare:
        return expression_from_declare(declare_clone(original.declare));

    case expression_type_integer_literal:
        return expression_from_integer_literal(integer_literal_expression_clone(original.integer_literal));

    case expression_type_string:
        return expression_from_string(string_expression_clone(original.string));

    case expression_type_tuple:
        return expression_from_tuple(tuple_clone(original.tuple));

    case expression_type_struct:
        return expression_from_struct(struct_expression_clone(original.struct_));

    case expression_type_instantiate_struct:
        return expression_from_instantiate_struct(instantiate_struct_expression_clone(original.instantiate_struct));

    case expression_type_comment:
        return expression_from_comment(comment_expression_clone(original.comment));

    case expression_type_loop:
        return expression_from_loop(sequence_clone(original.loop_body));

    case expression_type_break:
        return expression_from_break(original.source);

    case expression_type_enum:
        return expression_from_enum(enum_expression_clone(original.enum_));

    case expression_type_match:
        return expression_from_match(match_clone(original.match));

    case expression_type_not:
        return expression_from_not(not_clone(original.not));

    case expression_type_interface:
        return expression_from_interface(interface_expression_clone(original.interface_));

    case expression_type_impl:
        return expression_from_impl(impl_expression_clone(original.impl));

    case expression_type_placeholder:
        return expression_from_placeholder(placeholder_expression_clone(original.placeholder));

    case expression_type_type_of:
        return expression_from_type_of(type_of_expression_clone(original.type_of));

    case expression_type_return:
        return expression_from_return(expression_allocate(expression_clone(*original.return_)));

    case expression_type_binary:
    case expression_type_import:
        LPG_TO_DO();
    }
    LPG_UNREACHABLE();
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
    ASSUME(left);
    ASSUME(right);
    if (left->type != right->type)
    {
        return 0;
    }
    switch (left->type)
    {
    case expression_type_new_array:
        LPG_TO_DO();

    case expression_type_import:
        LPG_TO_DO();

    case expression_type_generic_instantiation:
        return generic_instantiation_expression_equals(left->generic_instantiation, right->generic_instantiation);

    case expression_type_type_of:
        LPG_TO_DO();

    case expression_type_placeholder:
        return placeholder_expression_equals(left->placeholder, right->placeholder);

    case expression_type_enum:
        return enum_expression_equals(left->enum_, right->enum_);

    case expression_type_instantiate_struct:
        return instantiate_struct_expression_equals(left->instantiate_struct, right->instantiate_struct);

    case expression_type_interface:
        return interface_expression_equals(left->interface_, right->interface_);

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
    case expression_type_string:
        LPG_TO_DO();

    case expression_type_access_structure:
        return access_structure_equals(left->access_structure, right->access_structure);

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

sequence sequence_clone(sequence const original)
{
    expression *const elements = allocate_array(original.length, sizeof(*elements));
    for (size_t i = 0; i < original.length; ++i)
    {
        elements[i] = expression_clone(original.elements[i]);
    }
    return sequence_create(elements, original.length);
}

expression expression_from_binary_operator(binary_operator_expression value)
{
    expression result;
    result.type = expression_type_binary;
    result.binary = value;
    return result;
}
