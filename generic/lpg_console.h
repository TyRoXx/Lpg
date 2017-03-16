#pragma once
#include "lpg_unicode_string.h"
#include "lpg_optional.h"

#if defined(__linux__) || defined(__APPLE__)
#define LPG_UNIX_CONSOLE 1
#else
#define LPG_UNIX_CONSOLE 0
#endif

typedef enum console_color
{
    console_color_red,
    console_color_grey,
    console_color_white,
    console_color_cyan,
    console_color_yellow
} console_color;

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

void console_print_char(console_printer *printer, unicode_code_point text,
                        console_color text_color);

void console_new_line(console_printer *printer);

void print_to_console(console_cell const *cells, size_t line_length,
                      size_t number_of_lines);

void prepare_console(void);

typedef enum virtual_key_type
{
    virtual_key_type_unicode,
    virtual_key_type_left,
    virtual_key_type_up,
    virtual_key_type_right,
    virtual_key_type_down,
    virtual_key_type_delete,
    virtual_key_type_backspace
} virtual_key_type;

typedef struct virtual_key
{
    virtual_key_type type;
    unicode_code_point unicode;
} virtual_key;

typedef enum key_state
{
    key_state_down,
    key_state_up
} key_state;

typedef struct key_event
{
    key_state main_state;
    virtual_key main_key;
    key_state control;
} key_event;

typedef struct optional_key_event
{
    optional state;
    key_event value;
} optional_key_event;

static optional_key_event const optional_key_event_empty = {
    optional_empty,
    {key_state_down, {virtual_key_type_left, 0}, key_state_down}};

static optional_key_event make_optional_key_event(key_event value)
{
    optional_key_event result = {optional_set, value};
    return result;
}

typedef struct optional_virtual_key
{
    optional state;
    virtual_key value;
} optional_virtual_key;

static optional_virtual_key const optional_key_empty = {
    optional_empty, {virtual_key_type_left, 0}};

typedef struct console_key_parser
{
#if LPG_UNIX_CONSOLE
    char sequence[2];
    char ignore_next;
#else
    char unused;
#endif
} console_key_parser;

optional_key_event wait_for_key_event(console_key_parser *parser);
