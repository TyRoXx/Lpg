#include "lpg_console.h"
#include "lpg_cursor.h"
#include "lpg_allocate.h"
#include <stdio.h>

typedef struct cursor_list
{
    cursor_link const *head;
    size_t size;
} cursor_list;

static cursor_list descend(cursor_list cursor, size_t const rendered_child)
{
    static cursor_list const empty_cursor_list = {NULL, 0};
    if (!cursor.head)
    {
        return empty_cursor_list;
    }
    if (cursor.head->child != rendered_child)
    {
        return empty_cursor_list;
    }
    --cursor.size;
    if (cursor.size == 0)
    {
        return empty_cursor_list;
    }
    ++cursor.head;
    return cursor;
}

void render_expression(console_printer *const printer,
                       expression const *const source, cursor_list const cursor)
{
    switch (source->type)
    {
    case expression_type_lambda:
        console_print_char(printer, '/', console_color_cyan);
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
            console_print_char(
                printer, (unicode_code_point)buffer[i], console_color_white);
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
            if (cursor.size == 1 && (cursor.head->child == i))
            {
                console_print_char(printer, '>', console_color_yellow);
            }
            /*TODO: decode UTF-8*/
            console_print_char(printer,
                               (unicode_code_point)source->utf8_literal.data[i],
                               console_color_white);
        }
        break;
    }
}

enum
{
    console_width = 80,
    console_height = 24
};

void render(expression const *source, cursor_list const cursors)
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

key_event_handler_result handle_key_event(key_event event,
                                          expression const *source,
                                          cursor_list *cursors)
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
    render(source, *cursors);
    return continue_running;
}

void run_editor(expression *source)
{
    cursor_link cursor_links[2] = {{1}, {3}};
    cursor_list cursors = {&cursor_links[0], 2};
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
