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
    prepare_win32_console();
    for (;;)
    {
        optional_key_event const key = wait_for_key_event();
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
