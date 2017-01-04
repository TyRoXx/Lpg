#pragma once
#include "lpg_for.h"
#include <Windows.h>
#include <stdlib.h>
#include <stdint.h>

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

inline unicode_code_point restrict_console_text(unicode_code_point original)
{
    if (original <= 31)
    {
        return '?';
    }
    return original;
}

inline void console_print_char(console_printer *printer,
                               unicode_code_point text,
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

inline void console_new_line(console_printer *printer)
{
    if (printer->current_line == printer->number_of_lines)
    {
        return;
    }
    printer->position_in_line = 0;
    ++printer->current_line;
}

inline WORD to_win32_console_color(console_color color)
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

inline void print_to_console(console_cell const *cells, size_t line_length,
                             size_t number_of_lines)
{
    HANDLE consoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    LPG_FOR(size_t, y, number_of_lines)
    {
        COORD position = {0, (SHORT)y};
        if (!SetConsoleCursorPosition(consoleOutput, position))
        {
            abort();
        }
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
    }
}
