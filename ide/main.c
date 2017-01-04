#include "lpg_console.h"
#include "lpg_expression.h"
#include "lpg_allocate.h"
#include <stdio.h>

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

typedef enum key_event_handler_result
{
    continue_running,
    stop_running
} key_event_handler_result;

typedef enum key
{
    key_s,
    key_escape
} key;

typedef enum key_state
{
    key_state_down,
    key_state_up
} key_state;

typedef enum optional
{
    optional_set,
    optional_empty
} optional;

typedef struct key_event
{
    key_state main_state;
    key main_key;
    key_state control;
} key_event;

typedef struct optional_key_event
{
    optional state;
    key_event value;
} optional_key_event;

optional_key_event const optional_key_event_empty = {
    optional_empty, {key_state_down, key_escape, key_state_down}};

optional_key_event make_optional_key_event(key_event value)
{
    optional_key_event result = {optional_set, value};
    return result;
}

typedef struct optional_key
{
    optional state;
    key value;
} optional_key;

optional_key const optional_key_empty = {optional_empty, key_escape};

optional_key make_optional_key(key value)
{
    optional_key result = {optional_set, value};
    return result;
}

optional_key from_win32_key(WORD code)
{
    switch (code)
    {
    case 27:
        return make_optional_key(key_escape);

    case 83:
        return make_optional_key(key_s);
    }
    return optional_key_empty;
}

optional_key_event from_win32_key_event(KEY_EVENT_RECORD event)
{
    optional_key const key = from_win32_key(event.wVirtualKeyCode);
    switch (key.state)
    {
    case optional_empty:
        return optional_key_event_empty;

    case optional_set:
    {
        key_event result = {
            event.bKeyDown ? key_state_down : key_state_up, key.value,
            (event.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))
                ? key_state_down
                : key_state_up};
        return make_optional_key_event(result);
    }
    }
    abort();
}

key_event_handler_result handle_key_event(key_event event,
                                          expression const *source)
{
    switch (event.main_state)
    {
    case key_state_down:
        return continue_running;

    case key_state_up:
        break;
    }
    if ((event.main_key == key_s) && (event.control == key_state_down))
    {
        printf("Save\n");
        return continue_running;
    }
    if (event.main_key == key_escape)
    {
        return stop_running;
    }
    render(source);
    return continue_running;
}

void run_editor(expression *source)
{
    render(source);
    HANDLE consoleInput = GetStdHandle(STD_INPUT_HANDLE);
    if (!SetConsoleMode(
            consoleInput, ENABLE_EXTENDED_FLAGS | ENABLE_INSERT_MODE))
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

        if (input.EventType == KEY_EVENT)
        {
            optional_key_event const key =
                from_win32_key_event(input.Event.KeyEvent);
            switch (key.state)
            {
            case optional_empty:
                break;

            case optional_set:
                switch (handle_key_event(key.value, source))
                {
                case continue_running:
                    break;

                case stop_running:
                    return;
                }
                break;
            }
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
    check_allocations();
    return 0;
}
