#include "lpg_create_process.h"
#include "lpg_assert.h"
#include "lpg_allocate.h"

#ifdef _WIN32
typedef struct win32_string
{
    WCHAR *c_str;
    size_t size;
} win32_string;

static void win32_string_free(win32_string const freed)
{
    if (freed.c_str)
    {
        deallocate(freed.c_str);
    }
}

static win32_string to_win32_path(unicode_view const original)
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

static win32_string build_command_line(unicode_view const executable, unicode_view const *const arguments,
                                       size_t const argument_count)
{
    size_t result_size = 2;
    {
        ASSERT(executable.length <= (size_t)INT_MAX);
        int const converted_size = MultiByteToWideChar(CP_UTF8, 0, executable.begin, (int)executable.length, NULL, 0);
        ASSERT((executable.length == 0) || (converted_size != 0));
        result_size += (size_t)converted_size;
    }
    for (size_t i = 0; i < argument_count; ++i)
    {
        result_size += 1;
        ASSERT(arguments[i].length <= (size_t)INT_MAX);
        int const converted_size =
            MultiByteToWideChar(CP_UTF8, 0, arguments[i].begin, (int)arguments[i].length, NULL, 0);
        ASSERT((arguments[i].length == 0) || (converted_size != 0));
        result_size += (size_t)converted_size;
    }
    ASSERT(result_size <= (size_t)INT_MAX);
    win32_string const result = {allocate_array(result_size + 1, sizeof(*result.c_str)), result_size};
    result.c_str[0] = L'"';
    size_t destination = 1;
    destination += (size_t)MultiByteToWideChar(CP_UTF8, 0, executable.begin, (int)executable.length,
                                               result.c_str + destination, (int)(result.size - destination));
    result.c_str[destination] = L'"';
    destination += 1;
    for (size_t i = 0; i < argument_count; ++i)
    {
        result.c_str[destination] = L' ';
        destination += 1;
        destination += (size_t)MultiByteToWideChar(CP_UTF8, 0, arguments[i].begin, (int)arguments[i].length,
                                                   result.c_str + destination, (int)(result.size - destination));
    }
    ASSUME(destination == result.size);
    result.c_str[result.size] = L'\0';
    return result;
}
#endif

create_process_result create_process_result_create(success_indicator success, child_process created)
{
    create_process_result const result = {success, created};
    return result;
}

create_process_result create_process(unicode_view const executable, unicode_view const *const arguments,
                                     size_t const argument_count, unicode_view const current_path,
                                     file_handle const input, file_handle const output, file_handle const error)
{
#ifdef _WIN32
    STARTUPINFOW startup;
    memset(&startup, 0, sizeof(startup));
    startup.cb = sizeof(startup);
    startup.dwFlags |= STARTF_USESTDHANDLES;
    startup.hStdError = error;
    startup.hStdInput = input;
    startup.hStdOutput = output;
    const DWORD flags = CREATE_NO_WINDOW;
    PROCESS_INFORMATION process;
    win32_string const executable_converted = to_win32_path(executable);
    win32_string const current_path_converted = to_win32_path(current_path);
    win32_string const command_line = build_command_line(executable, arguments, argument_count);
    BOOL const successful = CreateProcessW(executable_converted.c_str, command_line.c_str, NULL, NULL, TRUE, flags,
                                           NULL, current_path_converted.c_str, &startup, &process);
    win32_string_free(executable_converted);
    win32_string_free(current_path_converted);
    win32_string_free(command_line);
    if (successful)
    {
        CloseHandle(process.hThread);
        child_process const created = {process.hProcess};
        return create_process_result_create(success, created);
    }
    child_process const created = {INVALID_HANDLE_VALUE};
    return create_process_result_create(failure, created);
#else
    (void)executable;
    (void)arguments;
    (void)argument_count;
    (void)current_path;
    (void)input;
    (void)output;
    (void)error;
    child_process const created = {-1};
    return create_process_result_create(failure, created);
#endif
}
