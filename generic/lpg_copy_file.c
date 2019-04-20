#include "lpg_copy_file.h"
#include "lpg_assert.h"
#include "lpg_win32.h"
#ifdef _WIN32
#include <Windows.h>
#else
#include <stdio.h>
#endif

bool copy_file(unicode_view const from, unicode_view const to)
{
#ifdef _WIN32
    win32_string const from_zero_terminated = to_win32_path(from);
    win32_string const to_zero_terminated = to_win32_path(to);
    bool const result = CopyFileW(from_zero_terminated.c_str, to_zero_terminated.c_str, FALSE);
    win32_string_free(from_zero_terminated);
    win32_string_free(to_zero_terminated);
    return result;
#else
    unicode_string from_zero_terminated = unicode_view_zero_terminate(from);
    unicode_string to_zero_terminated = unicode_view_zero_terminate(to);
    FILE *const in = fopen(from_zero_terminated.data, "rb");
    bool result = false;
    if (in)
    {
        FILE *const out = fopen(to_zero_terminated.data, "wb");
        if (out)
        {
            char buffer[4096];
            for (;;)
            {
                size_t const read = fread(buffer, 1, sizeof(buffer), in);
                if (read == 0)
                {
                    result = feof(in);
                    break;
                }
                if (fwrite(buffer, 1, read, out) != read)
                {
                    ASSUME(!result);
                    break;
                }
            }
            fclose(out);
        }
        fclose(in);
    }
    unicode_string_free(&from_zero_terminated);
    unicode_string_free(&to_zero_terminated);
    return result;
#endif
}
