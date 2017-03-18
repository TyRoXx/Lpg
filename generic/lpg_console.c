#include "lpg_console.h"
#include "lpg_for.h"
#include "lpg_optional.h"
#include "lpg_unicode_string.h"
#ifdef _WIN32
#include <Windows.h>
#endif
#if LPG_UNIX_CONSOLE
#include <unistd.h>
#include <termios.h>
#endif
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static unicode_code_point restrict_console_text(unicode_code_point original)
{
    if (original <= 31)
    {
        return '?';
    }
    return original;
}

void console_print_char(console_printer *printer, unicode_code_point text,
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

void console_new_line(console_printer *printer)
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
        return FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
    case console_color_grey:
        return FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;
    case console_color_red:
        return FOREGROUND_RED | FOREGROUND_INTENSITY;
    case console_color_white:
        return FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED |
               FOREGROUND_INTENSITY;
    case console_color_yellow:
        return FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY;
    }
    abort();
}
#endif

#if LPG_UNIX_CONSOLE
static void write_stdout(char const *data, size_t size)
{
    ssize_t rc = write(STDOUT_FILENO, data, size);
    if (rc < 0)
    {
        abort();
    }
    if ((size_t)rc != size)
    {
        abort();
    }
}

#endif

void print_to_console(console_cell const *cells, size_t line_length,
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
    console_color previous_color = console_color_white;
    write_stdout("\033[37m", 5);
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
            if (cell->text_color != previous_color)
            {
                previous_color = cell->text_color;
                switch (previous_color)
                {
                case console_color_cyan:
                    write_stdout("\033[36m", 5);
                    break;

                case console_color_grey:
                    write_stdout("\033[37m", 5);
                    break;

                case console_color_red:
                    write_stdout("\033[31m", 5);
                    break;

                case console_color_white:
                    write_stdout("\033[37m", 5);
                    break;

                case console_color_yellow:
                    write_stdout("\033[33m", 5);
                    break;
                }
            }
            char c = (char)cell->text;
            /*assume ASCII for now*/
            assert((unicode_code_point)c == cell->text);
            write_stdout(&c, 1);
        }
    }
#endif
}

void prepare_console(void)
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

static optional_key_event make_optional_key_event(key_event value)
{
    optional_key_event result = {optional_set, value};
    return result;
}

#ifdef _WIN32
static virtual_key make_virtual_key(virtual_key_type type,
                                    unicode_code_point unicode)
{
    virtual_key result = {type, unicode};
    return result;
}
#endif
#ifdef _WIN32
static optional_virtual_key make_optional_virtual_key(virtual_key value)
{
    optional_virtual_key result = {optional_set, value};
    return result;
}

static optional_virtual_key from_win32_key(WORD code, WCHAR unicode)
{
    if (unicode != 0)
    {
        return make_optional_virtual_key(
            make_virtual_key(virtual_key_type_unicode, unicode));
    }
    switch (code)
    {
    case 8:
        return make_optional_virtual_key(
            make_virtual_key(virtual_key_type_backspace, 0));

    case 37:
        return make_optional_virtual_key(
            make_virtual_key(virtual_key_type_left, 0));

    case 38:
        return make_optional_virtual_key(
            make_virtual_key(virtual_key_type_up, 0));

    case 39:
        return make_optional_virtual_key(
            make_virtual_key(virtual_key_type_right, 0));

    case 40:
        return make_optional_virtual_key(
            make_virtual_key(virtual_key_type_down, 0));

    case 46:
        return make_optional_virtual_key(
            make_virtual_key(virtual_key_type_delete, 0));
    }
    char line[256];
    snprintf(line, sizeof(line), "Unhandled key code: %d\n", code);
    OutputDebugStringA(line);
    return optional_key_empty;
}

static optional_key_event from_win32_key_event(KEY_EVENT_RECORD event)
{
    /*TODO: handle event.wRepeatCount > 1*/
    optional_virtual_key const key =
        from_win32_key(event.wVirtualKeyCode, event.uChar.UnicodeChar);
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
static optional_key_event make_special_key(virtual_key_type const key)
{
    key_event result = {key_state_down, {key, 0}, key_state_up};
    return make_optional_key_event(result);
}

static optional_key_event parse_unix_console_key(console_key_parser *parser,
                                                 char input)
{
    if (parser->ignore_next)
    {
        parser->ignore_next = 0;
        return optional_key_event_empty;
    }
    if (parser->sequence[0])
    {
        if (parser->sequence[1])
        {
            memset(parser->sequence, 0, sizeof(parser->sequence));
            switch (input)
            {
            case 65:
                return make_special_key(virtual_key_type_up);

            case 66:
                return make_special_key(virtual_key_type_down);

            case 67:
                return make_special_key(virtual_key_type_right);

            case 68:
                return make_special_key(virtual_key_type_left);

            case 51:
                parser->ignore_next = 1;
                return make_special_key(virtual_key_type_delete);

            default:
                return optional_key_event_empty;
            }
        }
        else
        {
            switch (input)
            {
            case 91:
                parser->sequence[1] = input;
                return optional_key_event_empty;

            case 27:
            {
                memset(parser->sequence, 0, sizeof(parser->sequence));
                key_event result = {
                    key_state_up, {virtual_key_type_unicode, 27}, key_state_up};
                return make_optional_key_event(result);
            }

            default:
                memset(parser->sequence, 0, sizeof(parser->sequence));
                return optional_key_event_empty;
            }
        }
    }
    else
    {
        if (input == 27)
        {
            parser->sequence[0] = input;
            return optional_key_event_empty;
        }
        if (input == 127)
        {
            return make_special_key(virtual_key_type_backspace);
        }
        memset(parser->sequence, 0, sizeof(parser->sequence));
        key_event result = {key_state_down,
                            {virtual_key_type_unicode,
                             /*TODO: decode UTF-8*/ (unicode_code_point)input},
                            key_state_up};
        return make_optional_key_event(result);
    }
}
#endif

optional_key_event wait_for_key_event(console_key_parser *parser)
{
#ifdef _WIN32
    (void)parser;
    HANDLE const consoleInput = GetStdHandle(STD_INPUT_HANDLE);
    INPUT_RECORD input;
    DWORD inputEvents;
    if (!ReadConsoleInputW(consoleInput, &input, 1, &inputEvents))
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
    return parse_unix_console_key(parser, input[0]);
#endif
}
