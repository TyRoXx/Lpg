#pragma once
#include "lpg_enum_element_id.h"
#include "lpg_integer.h"
#include "lpg_source_location.h"
#include "lpg_unicode_string.h"
#include <stdbool.h>

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
function_header_tree function_header_tree_clone(function_header_tree const original);

typedef struct generic_parameter_list
{
    unicode_view *names;
    size_t count;
} generic_parameter_list;

typedef struct lambda
{
    generic_parameter_list generic_parameters;
    function_header_tree header;
    expression *result;
    source_location source;
} lambda;

lambda lambda_create(generic_parameter_list generic_parameters, function_header_tree header, expression *result,
                     source_location source);
void lambda_free(LPG_NON_NULL(lambda const *this));
bool lambda_equals(lambda const left, lambda const right);
lambda lambda_clone(lambda const original);

typedef enum expression_type {
    expression_type_lambda = 1,
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
    expression_type_comment,
    expression_type_interface,
    expression_type_struct,
    expression_type_impl,
    expression_type_instantiate_struct,
    expression_type_enum,
    expression_type_placeholder,
    expression_type_generic_instantiation,
    expression_type_type_of,
    expression_type_import,
    expression_type_new_array
} expression_type;

typedef struct tuple
{
    expression *elements;
    size_t length;
    source_location opening_brace;
} tuple;

tuple tuple_clone(tuple const original);

typedef struct call
{
    expression *callee;
    tuple arguments;
    source_location closing_parenthesis;
} call;

call call_create(LPG_NON_NULL(expression *callee), tuple arguments, source_location closing_parenthesis);
call call_clone(call const original);

typedef struct comment_expression
{
    unicode_string value;
    source_location source;
} comment_expression;

comment_expression comment_expression_clone(comment_expression const original);

typedef struct identifier_expression
{
    unicode_view value;
    source_location source;
} identifier_expression;

identifier_expression identifier_expression_clone(identifier_expression const original);

struct parameter
{
    identifier_expression name;
    expression *type;
};

parameter parameter_create(identifier_expression name, LPG_NON_NULL(expression *type));
void parameter_free(LPG_NON_NULL(parameter *value));
bool parameter_equals(parameter const left, parameter const right);
parameter parameter_clone(parameter const original);

typedef struct access_structure
{
    expression *object;
    identifier_expression member;
} access_structure;

access_structure access_structure_create(LPG_NON_NULL(expression *object), identifier_expression member);
access_structure access_structure_clone(access_structure const original);
bool access_structure_equals(access_structure const left, access_structure const right);

typedef struct match_case
{
    /*key_or_default==NULL for default*/
    expression *key_or_default;
    expression *action;
} match_case;

match_case match_case_create(expression *key_or_default, LPG_NON_NULL(expression *action));
void match_case_free(LPG_NON_NULL(match_case *value));
bool match_case_equals(match_case const left, match_case const right);
match_case match_case_clone(match_case const original);

typedef struct match
{
    source_location begin;
    expression *input;
    match_case *cases;
    size_t number_of_cases;
} match;

match match_create(source_location begin, LPG_NON_NULL(expression *input), match_case *cases, size_t number_of_cases);
match match_clone(match const original);

typedef struct not
{
    expression *expr;
}
not;

not not_expression_create(expression * value);
void not_free(LPG_NON_NULL(not const *expression));
not not_clone(not const original);

typedef enum binary_operator {
    less_than = 1,
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
    source_location begin;
} sequence;

sequence sequence_clone(sequence const original);

typedef struct declare
{
    identifier_expression name;
    expression *optional_type;
    expression *initializer;
} declare;

declare declare_clone(declare const original);

generic_parameter_list generic_parameter_list_create(unicode_view *names, size_t count);
void generic_parameter_list_free(generic_parameter_list const freed);
bool generic_parameter_list_equals(generic_parameter_list const left, generic_parameter_list const right);
generic_parameter_list generic_parameter_list_clone(generic_parameter_list const original);

typedef struct interface_expression_method
{
    identifier_expression name;
    function_header_tree header;
} interface_expression_method;

interface_expression_method interface_expression_method_create(identifier_expression name, function_header_tree header);
void interface_expression_method_free(interface_expression_method value);
bool interface_expression_method_equals(interface_expression_method const left,
                                        interface_expression_method const right);
interface_expression_method interface_expression_method_clone(interface_expression_method const original);

typedef struct interface_expression
{
    generic_parameter_list parameters;
    source_location source;
    interface_expression_method *methods;
    size_t method_count;
} interface_expression;

interface_expression interface_expression_create(generic_parameter_list parameters, source_location source,
                                                 interface_expression_method *methods, size_t method_count);
void interface_expression_free(interface_expression value);
bool interface_expression_equals(interface_expression const left, interface_expression const right);
interface_expression interface_expression_clone(interface_expression const original);

typedef struct struct_expression_element struct_expression_element;

typedef struct struct_expression
{
    generic_parameter_list generic_parameters;
    source_location source;
    struct_expression_element *elements;
    size_t element_count;
} struct_expression;

struct_expression struct_expression_create(generic_parameter_list generic_parameters, source_location source,
                                           struct_expression_element *elements, size_t element_count);
void struct_expression_free(LPG_NON_NULL(struct_expression const *const value));
bool struct_expression_equals(struct_expression const left, struct_expression const right);
struct_expression struct_expression_clone(struct_expression const original);

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
impl_expression_method impl_expression_method_clone(impl_expression_method const original);

typedef struct impl_expression
{
    source_location begin;
    generic_parameter_list generic_parameters;
    expression *interface_;
    expression *self;
    impl_expression_method *methods;
    size_t method_count;
} impl_expression;

impl_expression impl_expression_create(source_location begin, generic_parameter_list generic_parameters,
                                       expression *interface_, expression *self, impl_expression_method *methods,
                                       size_t method_count);
void impl_expression_free(impl_expression value);
bool impl_expression_equals(impl_expression const left, impl_expression const right);
impl_expression impl_expression_clone(impl_expression const original);

typedef struct placeholder_expression
{
    source_location where;
    unicode_view name;
} placeholder_expression;

placeholder_expression placeholder_expression_create(source_location where, unicode_view name);
void placeholder_expression_free(placeholder_expression const freed);
bool placeholder_expression_equals(placeholder_expression const left, placeholder_expression const right);
placeholder_expression placeholder_expression_clone(placeholder_expression const original);

typedef struct new_array_expression
{
    expression *element;
} new_array_expression;

new_array_expression new_array_expression_create(expression *element);
void new_array_expression_free(new_array_expression const freed);
bool new_array_expression_equals(new_array_expression const left, new_array_expression const right);
new_array_expression new_array_expression_clone(new_array_expression const original);

sequence sequence_create(expression *elements, size_t length, source_location begin);
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

identifier_expression identifier_expression_create(unicode_view value, source_location source);
void identifier_expression_free(LPG_NON_NULL(identifier_expression const *value));
bool identifier_expression_equals(identifier_expression const left, identifier_expression const right);

typedef struct string_expression
{
    unicode_view value;
    source_location source;
} string_expression;

string_expression string_expression_create(unicode_view value, source_location source);
void string_expression_free(LPG_NON_NULL(string_expression const *value));
string_expression string_expression_clone(string_expression const original);
bool string_expression_equals(string_expression const left, string_expression const right);

typedef struct integer_literal_expression
{
    integer value;
    source_location source;
} integer_literal_expression;

integer_literal_expression integer_literal_expression_create(integer value, source_location source);
bool integer_literal_expression_equals(integer_literal_expression const left, integer_literal_expression const right);
integer_literal_expression integer_literal_expression_clone(integer_literal_expression const original);

comment_expression comment_expression_create(unicode_string value, source_location source);

typedef struct instantiate_struct_expression
{
    expression *type;
    tuple arguments;
} instantiate_struct_expression;

instantiate_struct_expression instantiate_struct_expression_create(expression *const type, tuple arguments);
void instantiate_struct_expression_free(instantiate_struct_expression const freed);
instantiate_struct_expression instantiate_struct_expression_clone(instantiate_struct_expression const original);

typedef struct enum_expression_element
{
    unicode_string name;
    expression *state;
} enum_expression_element;

enum_expression_element enum_expression_element_create(unicode_string name, expression *state);
void enum_expression_element_free(enum_expression_element const freed);
bool enum_expression_element_equals(enum_expression_element const left, enum_expression_element const right);
enum_expression_element enum_expression_element_clone(enum_expression_element const original);

typedef struct enum_expression
{
    source_location begin;
    generic_parameter_list parameters;
    enum_expression_element *elements;
    enum_element_id element_count;
} enum_expression;

enum_expression enum_expression_create(source_location const begin, generic_parameter_list parameters,
                                       enum_expression_element *const elements, enum_element_id const element_count);
void enum_expression_free(enum_expression const freed);
bool enum_expression_equals(enum_expression const left, enum_expression const right);
enum_expression enum_expression_clone(enum_expression const original);

typedef struct generic_instantiation_expression
{
    expression *generic;
    expression *arguments;
    size_t count;
} generic_instantiation_expression;

generic_instantiation_expression generic_instantiation_expression_create(expression *generic, expression *arguments,
                                                                         size_t count);
void generic_instantiation_expression_free(generic_instantiation_expression const freed);
bool generic_instantiation_expression_equals(generic_instantiation_expression const left,
                                             generic_instantiation_expression const right);
generic_instantiation_expression
generic_instantiation_expression_clone(generic_instantiation_expression const original);

typedef struct type_of_expression
{
    source_location begin;
    expression *target;
} type_of_expression;

type_of_expression type_of_expression_create(source_location begin, expression *target);
void type_of_expression_free(type_of_expression const freed);
bool type_of_expression_equals(type_of_expression const left, type_of_expression const right);
type_of_expression type_of_expression_clone(type_of_expression const original);

typedef struct import_expression
{
    source_location begin;
    identifier_expression name;
} import_expression;

import_expression import_expression_create(source_location begin, identifier_expression name);
void import_expression_free(import_expression const freed);
bool import_expression_equals(import_expression const left, import_expression const right);

typedef struct break_expression
{
    source_location begin;
    expression *value;
} break_expression;

break_expression break_expression_create(source_location begin, expression *value);
void break_expression_free(break_expression const freed);
bool break_expression_equals(break_expression const left, break_expression const right);
break_expression break_expression_clone(break_expression const original);

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
        comment_expression comment;
        interface_expression interface_;
        struct_expression struct_;
        impl_expression impl;
        /*for monostate expressions like break:*/
        source_location source;
        instantiate_struct_expression instantiate_struct;
        enum_expression enum_;
        placeholder_expression placeholder;
        generic_instantiation_expression generic_instantiation;
        type_of_expression type_of;
        import_expression import;
        new_array_expression new_array;
        break_expression break_;
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
struct_expression_element struct_expression_element_clone(struct_expression_element const original);

expression expression_from_integer_literal(integer_literal_expression value);
void call_free(LPG_NON_NULL(call const *this));
void access_structure_free(access_structure const *this);
void match_free(LPG_NON_NULL(match const *this));
expression expression_from_lambda(lambda content);
expression expression_from_string(string_expression content);
expression expression_from_comment(comment_expression content);
expression expression_from_call(call content);
expression expression_from_identifier(identifier_expression identifier);
expression expression_from_break(break_expression const content);
expression expression_from_interface(interface_expression content);
expression expression_from_struct(struct_expression content);
expression expression_from_instantiate_struct(instantiate_struct_expression content);
expression expression_from_enum(enum_expression const content);
expression expression_from_placeholder(placeholder_expression const content);
expression expression_from_generic_instantiation(generic_instantiation_expression const content);
expression expression_from_type_of(type_of_expression const content);
expression expression_from_import(import_expression const content);
expression expression_from_new_array(new_array_expression const content);
expression *expression_allocate(expression content);
void expression_free(LPG_NON_NULL(expression const *this));
expression expression_clone(expression const original);
bool sequence_equals(sequence const left, sequence const right);
bool declare_equals(declare const left, declare const right);
bool match_equals(match const left, match const right);
bool expression_equals(LPG_NON_NULL(expression const *left), LPG_NON_NULL(expression const *right));
