#include "lpg_console.h"
#include "lpg_cursor.h"
#include "lpg_allocate.h"
#include <stdio.h>

typedef struct cursor_list
{
    cursor_link *head;
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

static console_color yellow_or(int const condition,
                               console_color const otherwise)
{
    if (condition)
    {
        return console_color_yellow;
    }
    return otherwise;
}

void render_expression(console_printer *const printer,
                       expression const *const source, cursor_list const cursor)
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

typedef enum horizontal_end
{
    horizontal_end_left,
    horizontal_end_right
} horizontal_end;

static void add_link(cursor_list *const cursors, size_t const child)
{
    ++cursors->size;
    cursors->head =
        reallocate(cursors->head, sizeof(*cursors->head) * cursors->size);
    cursors->head[cursors->size - 1].child = child;
}

static void enter_expression(expression const *source,
                             cursor_list *const cursors,
                             horizontal_end const entered_from)
{
    switch (source->type)
    {
    case expression_type_lambda:
    case expression_type_builtin:
        add_link(cursors, 0);
        break;

    case expression_type_call:
    case expression_type_local:
        abort();

    case expression_type_integer_literal:
        add_link(cursors, 0);
        break;

    case expression_type_integer_range:
    case expression_type_function:
    case expression_type_add_member:
    case expression_type_fill_structure:
    case expression_type_access_structure:
    case expression_type_add_to_variant:
    case expression_type_match:
    case expression_type_sequence:
    case expression_type_assignment:
        abort();

    case expression_type_utf8_literal:
        switch (entered_from)
        {
        case horizontal_end_left:
            add_link(cursors, 0);
            break;

        case horizontal_end_right:
            add_link(cursors, (source->utf8_literal.length - 1));
            break;
        }
        break;
    }
}

typedef enum editing_input_result
{
    editing_input_result_ok,
    editing_input_result_go_left,
    editing_input_result_go_right
} editing_input_result;

static editing_input_result handle_editing_input(key_event event,
                                                 expression const *source,
                                                 cursor_list *cursors,
                                                 size_t const level)
{
    switch (source->type)
    {
    case expression_type_lambda:
        if ((cursors->size - 1) == level)
        {
            switch (event.main_key)
            {
            case key_left:
                --cursors->size;
                return editing_input_result_go_left;

            case key_right:
                --cursors->size;
                return editing_input_result_go_right;

            default:
                return editing_input_result_ok;
            }
        }
        switch (cursors->head[level].child)
        {
        case 0:
            switch (handle_editing_input(
                event, source->lambda.parameter_type, cursors, level + 1))
            {
            case editing_input_result_go_left:
                return editing_input_result_ok;

            case editing_input_result_go_right:
                ++cursors->head[level].child;
                enter_expression(source->lambda.parameter_name, cursors,
                                 horizontal_end_left);
                return editing_input_result_ok;

            case editing_input_result_ok:
                return editing_input_result_ok;
            }
            abort();

        case 1:
            switch (handle_editing_input(
                event, source->lambda.parameter_name, cursors, level + 1))
            {
            case editing_input_result_go_left:
                --cursors->head[level].child;
                enter_expression(source->lambda.parameter_type, cursors,
                                 horizontal_end_right);
                return editing_input_result_ok;

            case editing_input_result_go_right:
                ++cursors->head[level].child;
                enter_expression(
                    source->lambda.result, cursors, horizontal_end_left);
                return editing_input_result_ok;

            case editing_input_result_ok:
                return editing_input_result_ok;
            }
            abort();

        case 2:
            switch (handle_editing_input(
                event, source->lambda.result, cursors, level + 1))
            {
            case editing_input_result_go_left:
                --cursors->head[level].child;
                enter_expression(source->lambda.parameter_name, cursors,
                                 horizontal_end_right);
                return editing_input_result_ok;

            case editing_input_result_go_right:
                return editing_input_result_go_right;

            case editing_input_result_ok:
                return editing_input_result_ok;
            }
            abort();

        default:
            abort();
        }

    case expression_type_builtin:
        switch (event.main_key)
        {
        case key_left:
            --cursors->size;
            return editing_input_result_go_left;

        case key_right:
            --cursors->size;
            return editing_input_result_go_right;

        default:
            return editing_input_result_ok;
        }

    case expression_type_call:
    case expression_type_local:
        abort();

    case expression_type_integer_literal:
        switch (event.main_key)
        {
        case key_left:
            --cursors->size;
            return editing_input_result_go_left;

        case key_right:
            --cursors->size;
            return editing_input_result_go_right;

        default:
            return editing_input_result_ok;
        }

    case expression_type_integer_range:
    case expression_type_function:
    case expression_type_add_member:
    case expression_type_fill_structure:
    case expression_type_access_structure:
    case expression_type_add_to_variant:
    case expression_type_match:
    case expression_type_sequence:
    case expression_type_assignment:
        abort();

    case expression_type_utf8_literal:
        assert((cursors->size - 1) == level);
        switch (event.main_key)
        {
        case key_left:
            if (cursors->head[level].child == 0)
            {
                --cursors->size;
                return editing_input_result_go_left;
            }
            --cursors->head[level].child;
            break;

        case key_right:
            ++cursors->head[level].child;
            if (cursors->head[level].child == source->utf8_literal.length)
            {
                --cursors->size;
                return editing_input_result_go_right;
            }
            break;

        default:
            /*ignore*/
            break;
        }
        return editing_input_result_ok;
    }
    abort();
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
