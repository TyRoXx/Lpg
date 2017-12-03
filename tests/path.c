#include "print_instruction.h"
#include "path.h"
#include "lpg_allocate.h"
#include <string.h>
#include "lpg_win32.h"
#ifdef _WIN32
#include <Windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

static bool is_separator(char c)
{
    return (c == '/') || (c == '\\');
}

unicode_view path_remove_leaf(unicode_view const full)
{
    if (full.length == 0)
    {
        return full;
    }
    unicode_view result = full;
    if (is_separator(result.begin[result.length - 1]))
    {
        if (result.length == 1)
        {
            return full;
        }
        --result.length;
    }
    for (;;)
    {
        if (result.length == 0)
        {
            break;
        }
        if (is_separator(result.begin[result.length - 1]))
        {
            if (result.length > 1)
            {
                --result.length;
            }
            break;
        }
        --result.length;
    }
    return result;
}

unicode_string path_combine(unicode_view const *begin, size_t count)
{
    size_t maximum_length = 1;
    for (size_t i = 0; i < count; ++i)
    {
        maximum_length += begin[i].length;
        maximum_length += 1;
    }
    unicode_string result = {allocate(maximum_length), 0};
    size_t next_write = 0;
    for (size_t i = 0; i < count; ++i)
    {
        unicode_view const piece = begin[i];
        memcpy(result.data + next_write, piece.begin, piece.length);
        next_write += piece.length;
        if ((i != (count - 1)))
        {
            result.data[next_write] = '/';
            ++next_write;
        }
    }
    result.data[next_write] = '\0';
    result.length = next_write;
    for (size_t i = 0; i < result.length; ++i)
    {
        if (result.data[i] == '\\')
        {
            result.data[i] = '/';
        }
    }
    return result;
}

unicode_string get_current_executable_path(void)
{
#ifdef _WIN32
    WCHAR buffer[1000];
    DWORD const length = GetModuleFileNameW(NULL, buffer, LPG_ARRAY_SIZE(buffer));
    ASSERT(length < LPG_ARRAY_SIZE(buffer));
    buffer[length] = L'\0';
    ASSERT(GetLastError() == 0);
    int const output_size = WideCharToMultiByte(CP_UTF8, 0, buffer, length, NULL, 0, NULL, NULL);
    ASSERT(GetLastError() == 0);
    unicode_string const result = {allocate_array((size_t)output_size, sizeof(*result.data)), (size_t)output_size};
    WideCharToMultiByte(CP_UTF8, 0, buffer, length, result.data, (int)result.length, NULL, NULL);
    ASSERT(GetLastError() == 0);
    return result;
#elif defined(__linux__)
    char buffer[1024];
    ssize_t const rc = readlink("/proc/self/exe", buffer, sizeof(buffer));
    if (rc < 0)
    {
        LPG_TO_DO();
    }
    return unicode_string_from_range(buffer, (size_t)rc);
#else
    char buffer[256];
    uint32_t length = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &length) != 0)
    {
        LPG_TO_DO();
    }
    return unicode_string_from_range(buffer, length);
#endif
}

bool file_exists(unicode_view const path)
{
#ifdef _WIN32
    win32_string const path_argument = to_win32_path(path);
    DWORD const attributes = GetFileAttributesW(path_argument.c_str);
    win32_string_free(path_argument);
    return (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY));
#else
    unicode_string const zero_terminated_path = unicode_view_zero_terminate(path);
    struct stat buffer;
    if (stat(zero_terminated_path.data, &buffer) < 0)
    {
        unicode_string_free(&zero_terminated_path);
        return false;
    }
    unicode_string_free(&zero_terminated_path);
    return S_ISDIR(buffer.st_mode);
#endif
}

success_indicator create_directory(unicode_view const path)
{
    if (file_exists(path))
    {
        return success;
    }
    {
        unicode_view const parent = path_remove_leaf(path);
        if ((parent.length > 0) && !file_exists(parent))
        {
            switch (create_directory(parent))
            {
            case failure:
                return failure;

            case success:
                break;
            }
        }
    }
#ifdef _WIN32
    win32_string const path_argument = to_win32_path(path);
    BOOL const result = CreateDirectoryW(path_argument.c_str, NULL);
    win32_string_free(path_argument);
    if (result)
    {
        return success;
    }
    return failure;
#else
    unicode_string const zero_terminated_path = unicode_view_zero_terminate(path);
    if (mkdir(zero_terminated_path.data, 0744) < 0)
    {
        unicode_string_free(&zero_terminated_path);
        return failure;
    }
    unicode_string_free(&zero_terminated_path);
    return success;
#endif
}
