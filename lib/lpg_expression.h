#pragma once
#include "lpg_unicode_string.h"

typedef struct expression expression;

static void expression_free(expression *this);

static void expression_deallocate(expression *this)
{
    expression_free(this);
    deallocate(this);
}

typedef struct lambda
{
    expression *parameter_type;
    expression *parameter_name;
    expression *result;
} lambda;

static void lambda_free(lambda *this)
{
    expression_deallocate(this->parameter_type);
    expression_deallocate(this->parameter_name);
    expression_deallocate(this->result);
}

typedef struct integer
{
    uint64_t high;
    uint64_t low;
} integer;

static integer integer_create(uint64_t high, uint64_t low)
{
    integer result = {high, low};
    return result;
}

typedef struct integer_range
{
    integer minimum;
    integer maximum;
} integer_range;

typedef enum expression_type
{
    expression_type_lambda,
    expression_type_builtin,
    expression_type_call,
    expression_type_local,
    expression_type_integer_literal,
    expression_type_integer_range,
    expression_type_function,
    expression_type_add_member,
    expression_type_fill_structure,
    expression_type_access_structure,
    expression_type_add_to_variant,
    expression_type_match,
    expression_type_sequence,
    expression_type_assignment,
    expression_type_string
} expression_type;

typedef enum builtin
{
    builtin_unit,
    builtin_empty_structure,
    builtin_empty_variant
} builtin;

typedef struct call
{
    expression *callee;
    expression *arguments;
    size_t number_of_arguments;
} call;

typedef struct function
{
    expression *result;
    expression *parameter;
} function;

static void function_free(function *this)
{
    expression_deallocate(this->result);
    expression_deallocate(this->parameter);
}

typedef struct add_member
{
    expression *base;
    expression *name;
    expression *type;
} add_member;

static void add_member_free(add_member *this)
{
    expression_deallocate(this->base);
    expression_deallocate(this->name);
    expression_deallocate(this->type);
}

typedef struct fill_structure
{
    expression *members;
    size_t number_of_members;
} fill_structure;

typedef struct access_structure
{
    expression *object;
    unicode_string member;
} access_structure;

typedef struct add_to_variant
{
    expression *base;
    expression *new_type;
} add_to_variant;

typedef struct match_case
{
    expression *key;
    expression *action;
} match_case;

typedef struct match
{
    expression *input;
    match_case *cases;
    size_t number_of_cases;
} match;

typedef struct sequence
{
    expression *elements;
    size_t number_of_elements;
} sequence;

typedef struct assignment
{
    expression *variable;
    expression *new_value;
} assignment;

struct expression
{
    expression_type type;
    union
    {
        lambda lambda;
        builtin builtin;
        call call;
        expression *local;
        integer integer_literal;
        integer_range integer_range;
        function function;
        add_member add_member;
        fill_structure fill_structure;
        access_structure access_structure;
        add_to_variant add_to_variant;
        match match;
        sequence sequence;
        assignment assignment;
        unicode_string string;
    };
};

static expression expression_from_builtin(builtin value)
{
    expression result;
    result.type = expression_type_builtin;
    result.builtin = value;
    return result;
}

static expression expression_from_integer_literal(integer value)
{
    expression result;
    result.type = expression_type_integer_literal;
    result.integer_literal = value;
    return result;
}

static void call_free(call *this)
{
    expression_deallocate(this->callee);
    LPG_FOR(size_t, i, this->number_of_arguments)
    {
        expression_free(this->arguments + i);
    }
    deallocate(this->arguments);
}

static void fill_structure_free(fill_structure *this)
{
    LPG_FOR(size_t, i, this->number_of_members)
    {
        expression_free(this->members + i);
    }
    deallocate(this->members);
}

static void access_structure_free(access_structure *this)
{
    expression_deallocate(this->object);
    unicode_string_free(&this->member);
}

static void add_to_variant_free(add_to_variant *this)
{
    expression_deallocate(this->base);
    expression_deallocate(this->new_type);
}

static void match_free(match *this)
{
    expression_deallocate(this->input);
    LPG_FOR(size_t, i, this->number_of_cases)
    {
        expression_free(this->cases[i].key);
        expression_free(this->cases[i].action);
    }
    deallocate(this->cases);
}

static void sequence_free(sequence *this)
{
    LPG_FOR(size_t, i, this->number_of_elements)
    {
        expression_free(this->elements + i);
    }
    deallocate(this->elements);
}

static void assignment_free(assignment *this)
{
    expression_deallocate(this->variable);
    expression_deallocate(this->new_value);
}

static expression expression_from_lambda(lambda lambda)
{
    expression result;
    result.type = expression_type_lambda;
    result.lambda = lambda;
    return result;
}

static expression expression_from_unicode_string(unicode_string value)
{
    expression result;
    result.type = expression_type_string;
    result.string = value;
    return result;
}

static expression *expression_allocate(expression value)
{
    expression *result = allocate(sizeof(*result));
    *result = value;
    return result;
}

static void expression_free(expression *this)
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
    }
}