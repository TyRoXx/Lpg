#include "lpg_rename_file.h"
#include "lpg_win32.h"
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

bool rename_file(unicode_view const from, unicode_view const to)
{
#ifdef _WIN32
    win32_string const from_zero_terminated = to_win32_path(from);
    win32_string const to_zero_terminated = to_win32_path(to);
    bool const result = MoveFileExW(from_zero_terminated.c_str, to_zero_terminated.c_str, MOVEFILE_REPLACE_EXISTING);
    win32_string_free(from_zero_terminated);
    win32_string_free(to_zero_terminated);
    return result;
#else
    unicode_string from_zero_terminated = unicode_view_zero_terminate(from);
    unicode_string to_zero_terminated = unicode_view_zero_terminate(to);
    bool const result = (rename(from_zero_terminated.data, to_zero_terminated.data) >= 0);
    unicode_string_free(&from_zero_terminated);
    unicode_string_free(&to_zero_terminated);
    return result;
#endif
}