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
    for (type counter = 0; counter < (count); ++counter)

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

typedef enum console_color
{
    console_color_red,
    console_color_grey,
    console_color_white,
    console_color_cyan
} console_color;

typedef uint32_t unicode_code_point;

typedef struct console_cell
{
    unicode_code_point text;
    console_color text_color;
} console_cell;

typedef struct console_printer
{
    console_cell *first_cell;
    size_t line_length;
    size_t number_of_lines;
    size_t current_line;
    size_t position_in_line;
} console_printer;

unicode_code_point restrict_console_text(unicode_code_point original)
{
    if (original <= 31)
    {
        return '?';
    }
    return original;
}

void console_print_char(console_printer *printer, unicode_code_point text,
                        console_color text_color)
{
    if (printer->current_line == printer->number_of_lines)
    {
        return;
    }
    if (printer->position_in_line == printer->line_length)
    {
        return;
    }
    console_cell *cell =
        printer->first_cell + ((printer->current_line * printer->line_length) +
                               printer->position_in_line);
    cell->text = restrict_console_text(text);
    cell->text_color = text_color;
    ++printer->position_in_line;
    if (printer->position_in_line == printer->line_length)
    {
        printer->position_in_line = 0;
        ++printer->current_line;
    }
}

void console_new_line(console_printer *printer)
{
    if (printer->current_line == printer->number_of_lines)
    {
        return;
    }
    printer->position_in_line = 0;
    ++printer->current_line;
}

WORD to_win32_console_color(console_color color)
{
    switch (color)
    {
    case console_color_cyan:
        return FOREGROUND_BLUE | FOREGROUND_INTENSITY;
    case console_color_grey:
        return FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;
    case console_color_red:
        return FOREGROUND_RED | FOREGROUND_INTENSITY;
    case console_color_white:
        return FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED |
               FOREGROUND_INTENSITY;
    }
    abort();
}

void print_to_console(console_cell const *cells, size_t line_length,
                      size_t number_of_lines)
{
    HANDLE consoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    LPG_FOR(size_t, y, number_of_lines)
    {
        LPG_FOR(size_t, x, line_length)
        {
            console_cell const *cell = cells + (y * line_length) + x;
            if (!SetConsoleTextAttribute(
                    consoleOutput, to_win32_console_color(cell->text_color)))
            {
                abort();
            }

            WCHAR c = (WCHAR)cell->text;
            DWORD written;
            if (!WriteConsoleW(consoleOutput, &c, 1, &written, NULL))
            {
                abort();
            }
        }
        COORD position = {0, (SHORT)(y + 1)};
        if (!SetConsoleCursorPosition(consoleOutput, position))
        {
            abort();
        }
    }
}

void render_expression(console_printer *printer, expression const *source)
{
    switch (source->type)
    {
    case expression_type_lambda:
        console_print_char(printer, '/', console_color_cyan);
        render_expression(printer, source->lambda.parameter_type);
        console_print_char(printer, ' ', console_color_grey);
        render_expression(printer, source->lambda.parameter_name);
        console_new_line(printer);
        console_print_char(printer, ' ', console_color_grey);
        render_expression(printer, source->lambda.result);
        break;

    case expression_type_builtin:
        switch (source->builtin)
        {
        case builtin_unit:
            console_print_char(printer, '(', console_color_grey);
            console_print_char(printer, ')', console_color_grey);
            break;

        case builtin_empty_structure:
            console_print_char(printer, '{', console_color_grey);
            console_print_char(printer, '}', console_color_grey);
            break;

        case builtin_empty_variant:
            console_print_char(printer, '[', console_color_grey);
            console_print_char(printer, ']', console_color_grey);
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
        LPG_FOR(size_t, i, sizeof(buffer) - 1)
        {
            console_print_char(printer, buffer[i], console_color_white);
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
            console_print_char(
                printer, source->utf8_literal.data[i], console_color_white);
        }
        break;
    }
}

enum
{
    console_width = 80,
    console_height = 24
};

void render(expression const *source)
{
    console_cell cells[console_width * console_height];
    LPG_FOR(size_t, y, console_height)
    {
        LPG_FOR(size_t, x, console_width)
        {
            console_cell *cell = &cells[(y * console_width) + x];
            cell->text = ' ';
            cell->text_color = console_color_white;
        }
    }
    console_printer printer = {&cells[0], console_width, console_height, 0, 0};
    render_expression(&printer, source);
    print_to_console(cells, console_width, console_height);
}

void run_editor(expression *source)
{
    render(source);
    HANDLE consoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE consoleInput = GetStdHandle(STD_INPUT_HANDLE);
    if (!SetConsoleMode(consoleInput, ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT |
                                          ENABLE_EXTENDED_FLAGS |
                                          ENABLE_INSERT_MODE))
    {
        abort();
    }

    for (;;)
    {
        INPUT_RECORD input;
        DWORD inputEvents;
        if (!ReadConsoleInput(consoleInput, &input, 1, &inputEvents))
        {
            abort();
        }

        switch (input.EventType)
        {
        case KEY_EVENT:
        {
            if (!input.Event.KeyEvent.bKeyDown)
            {
                if ((input.Event.KeyEvent.wVirtualKeyCode == 0x53) &&
                    (input.Event.KeyEvent.dwControlKeyState &
                     (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)))
                {
                    printf("Save\n");
                    break;
                }

                WCHAR c = input.Event.KeyEvent.uChar.UnicodeChar;
                switch (c)
                {
                case 13:
                {
                    CONSOLE_SCREEN_BUFFER_INFO consoleScreenBufferInfo;
                    if (!GetConsoleScreenBufferInfo(
                            consoleOutput, &consoleScreenBufferInfo))
                    {
                        abort();
                    }
                    const COORD zero = {0, 0};
                    DWORD written = 0;
                    if (!FillConsoleOutputCharacter(
                            consoleOutput, ' ',
                            consoleScreenBufferInfo.dwSize.X *
                                consoleScreenBufferInfo.dwSize.Y,
                            zero, &written))
                    {
                        abort();
                    }
                    if (!SetConsoleCursorPosition(consoleOutput, zero))
                    {
                        abort();
                    }
                    if (!SetConsoleTextAttribute(
                            consoleOutput, FOREGROUND_BLUE))
                    {
                        abort();
                    }
                    render(source);
                    if (!SetConsoleTextAttribute(
                            consoleOutput, FOREGROUND_BLUE | FOREGROUND_GREEN |
                                               FOREGROUND_RED))
                    {
                        abort();
                    }
                    break;
                }

                case 27:
                    return;

                default:
                    _putwch(c);
                    break;
                }
            }
            break;
        }

        case MOUSE_EVENT:
            break;

        case WINDOW_BUFFER_SIZE_EVENT:
            break;

        case FOCUS_EVENT:
            break;

        default:
            printf("Unknown event\n");
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
