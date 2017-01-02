#include <conio.h>
#include <Windows.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

static size_t active_allocations = 0;

void *allocate(size_t size)
{
    void *memory = malloc(size);
    if (!memory)
    {
        abort();
    }
    ++active_allocations;
    return memory;
}

void deallocate(void *memory)
{
    --active_allocations;
    free(memory);
}

#define LPG_FOR(type, counter, count)                                          \
    for (type counter = 0; counter < count; ++counter)

typedef struct expression expression;

void expression_free(expression *this);

void expression_deallocate(expression *this)
{
    expression_free(this);
    deallocate(this);
}

typedef struct utf8_string
{
    char *data;
    size_t length;
} utf8_string;

utf8_string utf8_string_from_c_str(const char *c_str)
{
    utf8_string result;
    result.length = strlen(c_str);
    result.data = allocate(result.length);
    memcpy(result.data, c_str, result.length);
    return result;
}

void utf8_string_free(utf8_string *s)
{
    deallocate(s->data);
}

typedef struct lambda
{
    expression *parameter_type;
    expression *parameter_name;
    expression *result;
} lambda;

void lambda_free(lambda *this)
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

integer integer_create(uint64_t high, uint64_t low)
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
    expression_type_utf8_literal
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

void function_free(function *this)
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

void add_member_free(add_member *this)
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
    utf8_string member;
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
        utf8_string utf8_literal;
    };
};

expression expression_from_builtin(builtin value)
{
    expression result = {expression_type_builtin};
    result.builtin = value;
    return result;
}

expression expression_from_integer_literal(integer value)
{
    expression result = {expression_type_integer_literal};
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
    utf8_string_free(&this->member);
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
    expression result = {expression_type_lambda};
    result.lambda = lambda;
    return result;
}

expression expression_from_utf8_string(utf8_string value)
{
    expression result = {expression_type_utf8_literal};
    result.utf8_literal = value;
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

    case expression_type_utf8_literal:
        utf8_string_free(&this->utf8_literal);
        break;
    }
}

void print_char(char c)
{
    _putch(c);
}

void render(expression const *source)
{
    switch (source->type)
    {
    case expression_type_lambda:
        print_char('/');
        render(source->lambda.parameter_type);
        print_char(' ');
        render(source->lambda.parameter_name);
        print_char('\n');
        print_char('\t');
        render(source->lambda.result);
        break;

    case expression_type_builtin:
        switch (source->builtin)
        {
        case builtin_unit:
            print_char('(');
            print_char(')');
            break;

        case builtin_empty_structure:
            print_char('{');
            print_char('}');
            break;

        case builtin_empty_variant:
            print_char('[');
            print_char(']');
            break;
        }
        break;
    case expression_type_call:
        abort();
        break;
    case expression_type_local:
        abort();
        break;

    case expression_type_integer_literal:
    {
        char buffer[2 + (4 * 8) + 1];
        if (snprintf(buffer, sizeof(buffer), "0x%08X%08X%08X%08X",
                     (unsigned)(source->integer_literal.high >> 32u),
                     (unsigned)source->integer_literal.high,
                     (unsigned)(source->integer_literal.low >> 32u),
                     (unsigned)source->integer_literal.low) !=
            (sizeof(buffer) - 1))
        {
            abort();
        }
        LPG_FOR(size_t, i, sizeof(buffer))
        {
            print_char(buffer[i]);
        }
        break;
    }

    case expression_type_integer_range:
        abort();
        break;
    case expression_type_function:
        abort();
        break;
    case expression_type_add_member:
        abort();
        break;
    case expression_type_fill_structure:
        abort();
        break;
    case expression_type_access_structure:
        abort();
        break;
    case expression_type_add_to_variant:
        abort();
        break;
    case expression_type_match:
        abort();
        break;
    case expression_type_sequence:
        abort();
        break;
    case expression_type_assignment:
        abort();
        break;
    case expression_type_utf8_literal:
        LPG_FOR(size_t, i, source->utf8_literal.length)
        {
            print_char(source->utf8_literal.data[i]);
        }
        break;
    }
}

void run_editor(expression *source)
{
    render(source);
    HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);
    for (;;)
    {
        wint_t c = _getwch();
        switch (c)
        {
        case 13:
        {
            CONSOLE_SCREEN_BUFFER_INFO consoleScreenBufferInfo;
            if (!GetConsoleScreenBufferInfo(console, &consoleScreenBufferInfo))
            {
                abort();
            }
            const COORD zero = {0, 0};
            DWORD written = 0;
            if (!FillConsoleOutputCharacter(
                    console, ' ', consoleScreenBufferInfo.dwSize.X *
                                      consoleScreenBufferInfo.dwSize.Y,
                    zero, &written))
            {
                abort();
            }
            if (!SetConsoleCursorPosition(console, zero))
            {
                abort();
            }
            render(source);
            break;
        }

        case 27:
            return;

        default:
            _putwch(c);
            break;
        }
    }
}

int main(void)
{
    lambda main = {expression_allocate(expression_from_builtin(builtin_unit)),
                   expression_allocate(expression_from_utf8_string(
                       utf8_string_from_c_str("argument"))),
                   expression_allocate(expression_from_integer_literal(
                       integer_create(0, 123)))};
    expression root = expression_from_lambda(main);
    run_editor(&root);
    expression_free(&root);
    assert(active_allocations == 0);
    return 0;
}
