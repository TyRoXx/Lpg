#include "lpg_cli.h"
#include <stdio.h>
#if LPG_WITH_VLD
#include <vld.h>
#endif
#include "lpg_allocate.h"
#include "lpg_win32.h"
#include <errno.h>
#ifdef _WIN32
#include <Windows.h>
#else
#include <unistd.h>
#endif

static success_indicator write_file(void *const user, char const *const data, size_t const length)
{
    if (fwrite(data, 1, length, user) == length)
    {
        return success_yes;
    }
    return success_no;
}

static stream_writer make_file_writer(LPG_NON_NULL(FILE *const file))
{
    stream_writer const result = {write_file, file};
    return result;
}

static unicode_string get_current_directory(void)
{
#ifdef _WIN32
    wchar_t buffer[300];
    DWORD const rc = GetCurrentDirectoryW(LPG_ARRAY_SIZE(buffer), buffer);
    if ((rc == 0) || (rc > LPG_ARRAY_SIZE(buffer)))
    {
        LPG_TO_DO();
    }
    return from_win32_string(win32_string_create(buffer, rc));
#else
    size_t const initial_size = 100;
    unicode_string result = {allocate(initial_size), initial_size};
    for (;;)
    {
        if (getcwd(result.data, result.length))
        {
            result.length = strlen(result.data);
            return result;
        }
        switch (errno)
        {
        case ERANGE:
            result.length *= 2;
            result.data = reallocate(result.data, result.length);
            break;

        default:
            LPG_TO_DO();
        }
    }
#endif
}

int main(int const argc, char **const argv)
{
    stream_writer const diagnostics = make_file_writer(stderr);
    unicode_string const current_directory = get_current_directory();
    int const result = run_cli(argc, argv, diagnostics, unicode_view_from_string(current_directory),
                               unicode_view_from_c_str(LPG_CLI_MODULE_DIRECTORY));
    unicode_string_free(&current_directory);
    return result;
}
