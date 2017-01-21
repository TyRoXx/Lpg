#include "lpg_console.h"
#include "lpg_cursor.h"
#include "lpg_allocate.h"
#include <stdio.h>

static console_color yellow_or(int const condition,
                               console_color const otherwise)
{
    if (condition)
    {
        return console_color_yellow;
    }
    return otherwise;
}

static void render_expression(console_printer *const printer,
                              expression const *const source,
                              cursor_list const cursor)
{
    int const is_selected = (cursor.size == 1);
    switch (source->type)
    {
    case expression_type_lambda:
        console_print_char(
            printer, '/', yellow_or(is_selected, console_color_cyan));
        render_expression(
            printer, source->lambda.parameter_type, descend(cursor, 0));
        console_print_char(printer, ' ', console_color_grey);
        render_expression(
            printer, source->lambda.parameter_name, descend(cursor, 1));
        console_new_line(printer);
        console_print_char(printer, ' ', console_color_grey);
        render_expression(printer, source->lambda.result, descend(cursor, 2));
        break;

    case expression_type_builtin:
        switch (source->builtin)
        {
        case builtin_unit:
            console_print_char(
                printer, '(', yellow_or(is_selected, console_color_grey));
            console_print_char(
                printer, ')', yellow_or(is_selected, console_color_grey));
            break;

        case builtin_empty_structure:
            console_print_char(
                printer, '{', yellow_or(is_selected, console_color_grey));
            console_print_char(
                printer, '}', yellow_or(is_selected, console_color_grey));
            break;

        case builtin_empty_variant:
            console_print_char(
                printer, '[', yellow_or(is_selected, console_color_grey));
            console_print_char(
                printer, ']', yellow_or(is_selected, console_color_grey));
            break;
        }
        break;
    case expression_type_call:
        abort();

    case expression_type_local:
        abort();

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
            console_print_char(printer, (unicode_code_point)buffer[i],
                               yellow_or(is_selected, console_color_white));
        }
        break;
    }

    case expression_type_integer_range:
        abort();

    case expression_type_function:
        abort();

    case expression_type_add_member:
        abort();

    case expression_type_fill_structure:
        abort();

    case expression_type_access_structure:
        abort();

    case expression_type_add_to_variant:
        abort();

    case expression_type_match:
        abort();

    case expression_type_sequence:
        abort();

    case expression_type_assignment:
        abort();

    case expression_type_utf8_literal:
        LPG_FOR(size_t, i, source->utf8_literal.length)
        {
            if (cursor.size == 1 && (cursor.head->child == i))
            {
                console_print_char(printer, '>', console_color_yellow);
            }
            /*TODO: decode UTF-8*/
            console_print_char(printer,
                               (unicode_code_point)source->utf8_literal.data[i],
                               console_color_white);
        }
        if (cursor.size == 1 &&
            (cursor.head->child == source->utf8_literal.length))
        {
            console_print_char(printer, '>', console_color_yellow);
        }
        break;
    }
}

enum
{
    console_width = 80,
    console_height = 24
};

static void render(expression const *source, cursor_list const cursors)
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
    render_expression(&printer, source, cursors);
    print_to_console(cells, console_width, console_height);
}

typedef enum key_event_handler_result
{
    continue_running,
    stop_running
} key_event_handler_result;

key_event_handler_result handle_key_event(key_event event, expression *source,
                                          cursor_list *cursors)
{
    const unicode_code_point escape = 27;
    if ((event.main_key.type == virtual_key_type_unicode) &&
        (event.main_key.unicode == escape))
    {
        switch (event.main_state)
        {
        case key_state_down:
            return continue_running;

        case key_state_up:
            return stop_running;
        }
    }
    switch (handle_editing_input(event, source, cursors, 0))
    {
    case editing_input_result_ok:
        break;

    case editing_input_result_go_right:
        enter_expression(source, cursors, horizontal_end_right);
        break;

    case editing_input_result_go_left:
        enter_expression(source, cursors, horizontal_end_left);
        break;
    }
    render(source, *cursors);
    return continue_running;
}

void run_editor(expression *source)
{
    cursor_list cursors = {allocate(sizeof(*cursors.head) * 2), 2};
    cursors.head[0].child = 1;
    cursors.head[1].child = 3;
    render(source, cursors);
    prepare_console();
    for (;;)
    {
        optional_key_event const key = wait_for_key_event();
        switch (key.state)
        {
        case optional_empty:
            break;

        case optional_set:
            switch (handle_key_event(key.value, source, &cursors))
            {
            case continue_running:
                break;

            case stop_running:
                deallocate(cursors.head);
                return;
            }
            break;
        }
    }
}

int main(void)
{
    lambda root_function = {
        expression_allocate(expression_from_builtin(builtin_unit)),
        expression_allocate(
            expression_from_utf8_string(utf8_string_from_c_str("argument"))),
        expression_allocate(
            expression_from_integer_literal(integer_create(0, 123)))};
    expression root = expression_from_lambda(root_function);
    run_editor(&root);
    expression_free(&root);
    check_allocations();
    return 0;
}
