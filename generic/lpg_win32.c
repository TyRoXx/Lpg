#include "lpg_win32.h"
#include "lpg_allocate.h"
#include "lpg_assert.h"

#ifdef _WIN32
#include <Windows.h>

win32_string win32_string_create(wchar_t *c_str, size_t size)
{
    win32_string const result = {c_str, size};
    return result;
}

void win32_string_free(win32_string const freed)
{
    if (freed.c_str)
    {
        deallocate(freed.c_str);
    }
}

win32_string to_win32_path(unicode_view const original)
{
    ASSERT(original.length <= (size_t)INT_MAX);
    int const output_size = MultiByteToWideChar(CP_UTF8, 0, original.begin, (int)original.length, NULL, 0);
    ASSERT((original.length == 0) || (output_size != 0));
    win32_string const result = {allocate_array(((size_t)output_size) + 1, sizeof(*result.c_str)), (size_t)output_size};
    ASSERT(MultiByteToWideChar(CP_UTF8, 0, original.begin, (int)original.length, result.c_str, (int)result.size) ==
           output_size);
    for (int i = 0; i < output_size; ++i)
    {
        if (result.c_str[i] == L'/')
        {
            result.c_str[i] = L'\\';
        }
    }
    result.c_str[result.size] = L'\0';
    return result;
}

unicode_string from_win32_string(win32_string const original)
{
    ASSERT(original.size <= (size_t)INT_MAX);
    int const output_size = WideCharToMultiByte(CP_UTF8, 0, original.c_str, (int)original.size, NULL, 0, NULL, NULL);
    ASSERT((original.size == 0) || (output_size != 0));
    unicode_string const result = {allocate(output_size), output_size};
    ASSERT(WideCharToMultiByte(CP_UTF8, 0, original.c_str, (int)original.size, result.data, (int)result.length, NULL,
                               NULL) == output_size);
    return result;
}

wchar_t *win32_string_two_null_c_str(win32_string *const string)
{
    string->c_str = reallocate(string->c_str, (string->size + 2) * sizeof(*string->c_str));
    string->c_str[string->size] = 0;
    string->c_str[string->size + 1] = 0;
    return string->c_str;
}
#endif
