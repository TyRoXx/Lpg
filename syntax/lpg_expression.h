#pragma once
#include "lpg_unicode_string.h"
#include "lpg_integer.h"
#include "lpg_source_location.h"
#include <stdbool.h>
#include "lpg_enum_element_id.h"

typedef struct expression expression;
typedef struct parameter parameter;

void expression_deallocate(expression *this);

typedef struct function_header_tree
{
    parameter *parameters;
    size_t parameter_count;
    expression *return_type;
} function_header_tree;

function_header_tree function_header_tree_create(parameter *parameters, size_t parameter_count,
                                                 expression *return_type);
void function_header_tree_free(function_header_tree value);
bool function_header_tree_equals(function_header_tree const left, function_header_tree const right);

typedef struct lambda
{
    function_header_tree header;
    expression *result;
} lambda;

lambda lambda_create(function_header_tree header, expression *result);
void lambda_free(LPG_NON_NULL(lambda const *this));
bool lambda_equals(lambda const left, lambda const right);

typedef enum expression_type
{
    expression_type_lambda,
    expression_type_call,
    expression_type_integer_literal,
    expression_type_access_structure,
    expression_type_match,
    expression_type_string,
    expression_type_identifier,
    expression_type_not,
    expression_type_binary,
    expression_type_return,
    expression_type_loop,
    expression_type_break,
    expression_type_sequence,
    expression_type_declare,
    expression_type_tuple,
    expression_type_comment,
    expression_type_interface,
    expression_type_struct,
    expression_type_impl,
    expression_type_instantiate_struct,
    expression_type_enum,
    expression_type_placeholder
} expression_type;

typedef struct tuple
{
    expression *elements;
    size_t length;
    source_location opening_brace;
} tuple;

typedef struct call
{
    expression *callee;
    tuple arguments;
    source_location closing_parenthesis;
} call;

call call_create(LPG_NON_NULL(expression *callee), tuple arguments, source_location closing_parenthesis);

typedef struct comment_expression
{
    unicode_string value;
    source_location source;
} comment_expression;

typedef struct identifier_expression
{
    unicode_string value;
    source_location source;
} identifier_expression;

struct parameter
{
    identifier_expression name;
    expression *type;
};

parameter parameter_create(identifier_expression name, LPG_NON_NULL(expression *type));
void parameter_free(LPG_NON_NULL(parameter *value));
bool parameter_equals(parameter const left, parameter const right);

typedef struct access_structure
{
    expression *object;
    identifier_expression member;
} access_structure;

access_structure access_structure_create(LPG_NON_NULL(expression *object), identifier_expression member);

typedef struct match_case
{
    expression *key;
    expression *action;
} match_case;

match_case match_case_create(LPG_NON_NULL(expression *key), LPG_NON_NULL(expression *action));
void match_case_free(LPG_NON_NULL(match_case *value));
bool match_case_equals(match_case const left, match_case const right);

typedef struct match
{
    source_location begin;
    expression *input;
    match_case *cases;
    size_t number_of_cases;
} match;

match match_create(source_location begin, LPG_NON_NULL(expression *input), LPG_NON_NULL(match_case *cases),
                   size_t number_of_cases);

typedef struct not
{
    expression *expr;
}
not;

not not_expression_create(expression * value);
void not_free(LPG_NON_NULL(not const *expression));

typedef enum binary_operator
{
    less_than,
    less_than_or_equals,
    equals,
    greater_than,
    greater_than_or_equals,
    not_equals
} binary_operator;

typedef struct binary_operator_expression
{
    expression *left;
    expression *right;

    binary_operator comparator;
} binary_operator_expression;

binary_operator_expression binary_operator_expression_create(LPG_NON_NULL(expression *left),
                                                             LPG_NON_NULL(expression *right),
                                                             binary_operator anOperator);
void binary_operator_expression_free(binary_operator_expression const *value);

typedef struct sequence
{
    expression *elements;
    size_t length;
} sequence;

typedef struct declare
{
    identifier_expression name;
    expression *optional_type;
    expression *initializer;
} declare;

typedef struct interface_expression_method
{
    identifier_expression name;
    function_header_tree header;
} interface_expression_method;

interface_expression_method interface_expression_method_create(identifier_expression name, function_header_tree header);
void interface_expression_method_free(interface_expression_method value);
bool interface_expression_method_equals(interface_expression_method const left,
                                        interface_expression_method const right);

typedef struct interface_expression
{
    source_location source;
    interface_expression_method *methods;
    size_t method_count;
} interface_expression;

interface_expression interface_expression_create(source_location source, interface_expression_method *methods,
                                                 size_t method_count);
void interface_expression_free(interface_expression value);
bool interface_expression_equals(interface_expression const left, interface_expression const right);

typedef struct struct_expression_element struct_expression_element;

typedef struct struct_expression
{
    source_location source;
    struct_expression_element *elements;
    size_t element_count;
} struct_expression;

struct_expression struct_expression_create(source_location source, struct_expression_element *elements,
                                           size_t element_count);
void struct_expression_free(LPG_NON_NULL(struct_expression const *const value));
bool struct_expression_equals(struct_expression const left, struct_expression const right);

typedef struct impl_expression_method
{
    identifier_expression name;
    function_header_tree header;
    sequence body;
} impl_expression_method;

impl_expression_method impl_expression_method_create(identifier_expression name, function_header_tree header,
                                                     sequence body);
void impl_expression_method_free(impl_expression_method value);
bool impl_expression_method_equals(impl_expression_method const left, impl_expression_method const right);

typedef struct impl_expression
{
    expression *interface;
    expression *self;
    impl_expression_method *methods;
    size_t method_count;
} impl_expression;

impl_expression impl_expression_create(expression *interface, expression *self, impl_expression_method *methods,
                                       size_t method_count);
void impl_expression_free(impl_expression value);
bool impl_expression_equals(impl_expression const left, impl_expression const right);

typedef struct placeholder_expression
{
    source_location where;
    unicode_string name;
} placeholder_expression;

placeholder_expression placeholder_expression_create(source_location where, unicode_string name);
void placeholder_expression_free(placeholder_expression const freed);
bool placeholder_expression_equals(placeholder_expression const left, placeholder_expression const right);

sequence sequence_create(expression *elements, size_t length);
void sequence_free(LPG_NON_NULL(sequence const *value));
declare declare_create(identifier_expression name, expression *optional_type, LPG_NON_NULL(expression *initializer));
void declare_free(declare const *value);
tuple tuple_create(expression *elements, size_t length, source_location const opening_brace);
bool tuple_equals(tuple const *left, tuple const *right);
void tuple_free(LPG_NON_NULL(tuple const *value));
expression expression_from_not(not value);
expression expression_from_binary_operator(binary_operator_expression value);
expression expression_from_return(LPG_NON_NULL(expression *value));
expression expression_from_loop(sequence body);
expression expression_from_sequence(sequence value);
expression expression_from_access_structure(access_structure value);
expression expression_from_declare(declare value);
expression expression_from_match(match value);
expression expression_from_impl(impl_expression value);
source_location expression_source_begin(expression const value);

identifier_expression identifier_expression_create(unicode_string value, source_location source);
void identifier_expression_free(LPG_NON_NULL(identifier_expression const *value));
bool identifier_expression_equals(identifier_expression const left, identifier_expression const right);

typedef struct string_expression
{
    unicode_string value;
    source_location source;
} string_expression;

string_expression string_expression_create(unicode_string value, source_location source);
void string_expression_free(LPG_NON_NULL(string_expression const *value));

typedef struct integer_literal_expression
{
    integer value;
    source_location source;
} integer_literal_expression;

integer_literal_expression integer_literal_expression_create(integer value, source_location source);
bool integer_literal_expression_equals(integer_literal_expression const left, integer_literal_expression const right);

comment_expression comment_expression_create(unicode_string value, source_location source);

typedef struct instantiate_struct_expression
{
    expression *type;
    tuple arguments;
} instantiate_struct_expression;

instantiate_struct_expression instantiate_struct_expression_create(expression *const type, tuple arguments);
void instantiate_struct_expression_free(instantiate_struct_expression const freed);

typedef struct enum_expression_element
{
    unicode_string name;
    expression *state;
} enum_expression_element;

enum_expression_element enum_expression_element_create(unicode_string name, expression *state);
void enum_expression_element_free(enum_expression_element const freed);
bool enum_expression_element_equals(enum_expression_element const left, enum_expression_element const right);

typedef struct enum_expression
{
    source_location begin;
    enum_expression_element *elements;
    enum_element_id element_count;
} enum_expression;

enum_expression enum_expression_create(source_location begin, enum_expression_element *elements,
                                       enum_element_id element_count);
void enum_expression_free(enum_expression const freed);
bool enum_expression_equals(enum_expression const left, enum_expression const right);

struct expression
{
    expression_type type;
    union
    {
        lambda lambda;
        call call;
        integer_literal_expression integer_literal;
        access_structure access_structure;
        match match;
        string_expression string;
        identifier_expression identifier;
        not not;
        binary_operator_expression binary;
        expression *return_;
        sequence loop_body;
        sequence sequence;
        declare declare;
        tuple tuple;
        comment_expression comment;
        interface_expression interface;
        struct_expression struct_;
        impl_expression impl;
        /*for monostate expressions like break:*/
        source_location source;
        instantiate_struct_expression instantiate_struct;
        enum_expression enum_;
        placeholder_expression placeholder;
    };
};

struct struct_expression_element
{
    identifier_expression name;
    expression type;
};

struct_expression_element struct_expression_element_create(identifier_expression name, expression type);
void struct_expression_element_free(LPG_NON_NULL(struct_expression_element const *const value));
bool struct_expression_element_equals(struct_expression_element const left, struct_expression_element const right);

expression expression_from_integer_literal(integer_literal_expression value);
void call_free(LPG_NON_NULL(call const *this));
void access_structure_free(access_structure const *this);
void match_free(LPG_NON_NULL(match const *this));
expression expression_from_lambda(lambda lambda);
expression expression_from_string(string_expression value);
expression expression_from_comment(comment_expression value);
expression expression_from_call(call value);
expression expression_from_identifier(identifier_expression identifier);
expression expression_from_tuple(tuple value);
expression expression_from_break(source_location source);
expression expression_from_interface(interface_expression value);
expression expression_from_struct(struct_expression value);
expression expression_from_instantiate_struct(instantiate_struct_expression value);
expression expression_from_enum(enum_expression const value);
expression expression_from_placeholder(placeholder_expression const value);
expression *expression_allocate(expression value);
void expression_free(LPG_NON_NULL(expression const *this));
bool sequence_equals(sequence const left, sequence const right);
bool declare_equals(declare const left, declare const right);
bool match_equals(match const left, match const right);
bool expression_equals(LPG_NON_NULL(expression const *left), LPG_NON_NULL(expression const *right));
