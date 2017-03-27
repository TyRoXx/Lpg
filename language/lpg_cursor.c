#include "lpg_cursor.h"
#include "lpg_allocate.h"
#include <stdlib.h>
#include <assert.h>

cursor_list descend(cursor_list cursor, size_t const rendered_child)
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

static void add_link(cursor_list *const cursors, size_t const child)
{
    ++cursors->size;
    cursors->head =
        reallocate_array(cursors->head, cursors->size, sizeof(*cursors->head));
    cursors->head[cursors->size - 1].child = child;
}

void enter_expression(expression const *source, cursor_list *const cursors,
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
    case expression_type_assignment:
        abort();

    case expression_type_string:
        switch (entered_from)
        {
        case horizontal_end_left:
            add_link(cursors, 0);
            break;

        case horizontal_end_right:
            add_link(cursors, source->string.length);
            break;
        }
        break;

    case expression_type_identifier:
    case expression_assign:
    case expression_return:
    case expression_loop:
    case expression_break:
    case expression_sequence:
        abort();
    }
}

editing_input_result handle_editing_input(key_event event, expression *source,
                                          cursor_list *cursors,
                                          size_t const level)
{
    switch (event.main_state)
    {
    case key_state_up:
        return editing_input_result_ok;

    case key_state_down:
        break;
    }
    switch (source->type)
    {
    case expression_type_lambda:
        if ((cursors->size - 1) == level)
        {
            switch (event.main_key.type)
            {
            case virtual_key_type_left:
                --cursors->size;
                return editing_input_result_go_left;

            case virtual_key_type_right:
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
        switch (event.main_key.type)
        {
        case virtual_key_type_left:
            --cursors->size;
            return editing_input_result_go_left;

        case virtual_key_type_right:
            --cursors->size;
            return editing_input_result_go_right;

        default:
            return editing_input_result_ok;
        }

    case expression_type_call:
    case expression_type_local:
        abort();

    case expression_type_integer_literal:
        switch (event.main_key.type)
        {
        case virtual_key_type_left:
            --cursors->size;
            return editing_input_result_go_left;

        case virtual_key_type_right:
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
    case expression_type_assignment:
        abort();

    case expression_type_string:
        assert((cursors->size - 1) == level);
        switch (event.main_key.type)
        {
        case virtual_key_type_left:
            if (cursors->head[level].child == 0)
            {
                --cursors->size;
                return editing_input_result_go_left;
            }
            --cursors->head[level].child;
            break;

        case virtual_key_type_right:
            ++cursors->head[level].child;
            if (cursors->head[level].child > source->string.length)
            {
                --cursors->size;
                return editing_input_result_go_right;
            }
            break;

        case virtual_key_type_delete:
            if (cursors->head[level].child < source->string.length)
            {
                unicode_string_erase(
                    &source->string, cursors->head[level].child);
            }
            break;

        case virtual_key_type_backspace:
            if (cursors->head[level].child > 0)
            {
                unicode_string_erase(
                    &source->string, cursors->head[level].child - 1);
                --cursors->head[level].child;
            }
            break;

        case virtual_key_type_unicode:
            switch (event.main_key.unicode)
            {
            case 10:
                break;

            default:
                unicode_string_insert(&source->string,
                                      cursors->head[level].child,
                                      event.main_key.unicode);
                ++cursors->head[level].child;
                break;
            }
            break;

        default:
            /*ignore*/
            break;
        }
        return editing_input_result_ok;
    case expression_type_identifier:
        abort();
    }
    abort();
}
