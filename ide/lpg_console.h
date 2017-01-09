#pragma once
#include "lpg_for.h"
#ifdef _WIN32
#include <Windows.h>
#endif
#if defined(__linux__) || defined(__APPLE__)
#define LPG_UNIX_CONSOLE 1
#else
#define LPG_UNIX_CONSOLE 0
#endif
#if LPG_UNIX_CONSOLE
#include <unistd.h>
#include <termios.h>
#endif
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

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

static unicode_code_point restrict_console_text(unicode_code_point original)
{
    if (original <= 31)
    {
        return '?';
    }
    return original;
}

static void console_print_char(console_printer *printer,
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

static void console_new_line(console_printer *printer)
{
    if (printer->current_line == printer->number_of_lines)
    {
        return;
    }
    printer->position_in_line = 0;
    ++printer->current_line;
}

#ifdef _WIN32
static WORD to_win32_console_color(console_color color)
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
#endif

static void print_to_console(console_cell const *cells, size_t line_length,
                             size_t number_of_lines)
{
#ifdef _WIN32
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
#endif

#if LPG_UNIX_CONSOLE
    static char const clear[] = "\033[H\033[J";
    if (write(STDOUT_FILENO, &clear[0], sizeof(clear) - 1) !=
        (sizeof(clear) - 1))
    {
        abort();
    }
    LPG_FOR(size_t, y, number_of_lines)
    {
        if (y != 0)
        {
            char const new_line = '\n';
            if (write(STDOUT_FILENO, &new_line, 1) != 1)
            {
                abort();
            }
        }
        LPG_FOR(size_t, x, line_length)
        {
            console_cell const *cell = cells + (y * line_length) + x;
            char c = (char)cell->text;
            /*assume ASCII for now*/
            assert((unicode_code_point)c == cell->text);
            if (write(STDOUT_FILENO, &c, 1) != 1)
            {
                abort();
            }
        }
    }
#endif
}

static void prepare_console(void)
{
#ifdef _WIN32
    HANDLE consoleInput = GetStdHandle(STD_INPUT_HANDLE);
    if (!SetConsoleMode(
            consoleInput, ENABLE_EXTENDED_FLAGS | ENABLE_INSERT_MODE))
    {
        abort();
    }
#endif

#if LPG_UNIX_CONSOLE
    struct termios old_tio;
    struct termios new_tio;
    if (tcgetattr(STDIN_FILENO, &old_tio) != 0)
    {
        abort();
    }
    new_tio = old_tio;
    new_tio.c_lflag &= (tcflag_t)(~ICANON & ~ECHO);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &new_tio) != 0)
    {
        abort();
    }
#endif
}

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

static optional_key_event make_optional_key_event(key_event value)
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

#ifdef _WIN32
static optional_key make_optional_key(key value)
{
    optional_key result = {optional_set, value};
    return result;
}

static optional_key from_win32_key(WORD code)
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

static optional_key_event from_win32_key_event(KEY_EVENT_RECORD event)
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
#endif

#if LPG_UNIX_CONSOLE
static optional_key_event from_unix_key_event(char input)
{
    if (input == 27)
    {
        key_event result = {key_state_up, key_escape, key_state_up};
        return make_optional_key_event(result);
    }
    return optional_key_event_empty;
}
#endif

static optional_key_event wait_for_key_event(void)
{
#ifdef _WIN32
    HANDLE const consoleInput = GetStdHandle(STD_INPUT_HANDLE);
    INPUT_RECORD input;
    DWORD inputEvents;
    if (!ReadConsoleInput(consoleInput, &input, 1, &inputEvents))
    {
        abort();
    }
    if (input.EventType == KEY_EVENT)
    {
        return from_win32_key_event(input.Event.KeyEvent);
    }
    return optional_key_event_empty;
#endif

#if LPG_UNIX_CONSOLE
    char input[1];
    ssize_t bytes_read = read(STDIN_FILENO, input, sizeof(input));
    if (bytes_read < 0)
    {
        abort();
    }
    return from_unix_key_event(input[0]);
#endif
}
