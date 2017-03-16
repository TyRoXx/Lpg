#pragma once
#include "lpg_expression.h"
#include "lpg_console.h"

typedef struct cursor_link
{
    size_t child;
} cursor_link;

typedef enum horizontal_end
{
    horizontal_end_left,
    horizontal_end_right
} horizontal_end;

typedef struct cursor_list
{
    cursor_link *head;
    size_t size;
} cursor_list;

cursor_list descend(cursor_list cursor, size_t const rendered_child);
void add_link(cursor_list *const cursors, size_t const child);
void enter_expression(expression const *source, cursor_list *const cursors,
                      horizontal_end const entered_from);

typedef enum editing_input_result
{
    editing_input_result_ok,
    editing_input_result_go_left,
    editing_input_result_go_right
} editing_input_result;

editing_input_result handle_editing_input(key_event event, expression *source,
                                          cursor_list *cursors,
                                          size_t const level);
