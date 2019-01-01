#include "lpg_create_process.h"
#include "lpg_allocate.h"
#include "lpg_assert.h"
#include "lpg_win32.h"

#ifdef _WIN32
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
    for (size_t i = 1; i < destination; ++i)
    {
        if (result.c_str[i] == L'/')
        {
            result.c_str[i] = L'\\';
        }
    }
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
#else
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#ifdef __linux__
#include <sys/prctl.h>
#endif

file_handle get_standard_input()
{
#ifdef _WIN32
    return GetStdHandle(STD_INPUT_HANDLE);
#else
    return STDIN_FILENO;
#endif
}

file_handle get_standard_output()
{
#ifdef _WIN32
    return GetStdHandle(STD_OUTPUT_HANDLE);
#else
    return STDOUT_FILENO;
#endif
}

file_handle get_standard_error()
{
#ifdef _WIN32
    return GetStdHandle(STD_ERROR_HANDLE);
#else
    return STDERR_FILENO;
#endif
}

create_process_result create_process_result_create(success_indicator is_success, child_process created)
{
    create_process_result const result = {is_success, created};
    return result;
}

#ifndef _WIN32
typedef struct pipe_handle
{
    file_handle read;
    file_handle write;
} pipe_handle;

static pipe_handle make_pipe(void)
{
    int fds[2];
    if (pipe(fds) != 0)
    {
        LPG_TO_DO();
    }
    pipe_handle const result = {fds[0], fds[1]};
    return result;
}

static void ignore_ssize(ssize_t ignored)
{
    (void)ignored;
}
#endif

create_process_result create_process(unicode_view const executable, unicode_view const *const arguments,
                                     size_t const argument_count, unicode_view const current_path,
                                     file_handle const input, file_handle const output, file_handle const error_output)
{
#ifdef _WIN32
    STARTUPINFOW startup;
    memset(&startup, 0, sizeof(startup));
    startup.cb = sizeof(startup);
    startup.dwFlags |= STARTF_USESTDHANDLES;
    startup.hStdError = error_output;
    startup.hStdInput = input;
    startup.hStdOutput = output;
    const DWORD flags = 0;
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
        return create_process_result_create(success_yes, created);
    }
    child_process const created = {INVALID_HANDLE_VALUE};
    return create_process_result_create(success_no, created);
#else
    unicode_string const current_path_zero_terminated = unicode_view_zero_terminate(current_path);
    char **const exec_arguments = allocate_array(argument_count + 2, sizeof(*exec_arguments));
    unicode_string const executable_zero_terminated = unicode_view_zero_terminate(executable);
    exec_arguments[0] = executable_zero_terminated.data;
    for (size_t i = 0; i < argument_count; ++i)
    {
        unicode_string const argument_zero_terminated = unicode_view_zero_terminate(arguments[i]);
        exec_arguments[1 + i] = argument_zero_terminated.data;
    }
    exec_arguments[argument_count + 1] = NULL;
    pipe_handle const error_pipe = make_pipe();
    pid_t const forked = fork();
    if (forked == 0)
    {
        if (dup2(output, STDOUT_FILENO) < 0)
        {
            int const error = errno;
            ignore_ssize(write(error_pipe.write, &error, sizeof(error)));
            exit(1);
        }
        if (dup2(error_output, STDERR_FILENO) < 0)
        {
            int const error = errno;
            ignore_ssize(write(error_pipe.write, &error, sizeof(error)));
            exit(1);
        }
        if (dup2(input, STDIN_FILENO) < 0)
        {
            int const error = errno;
            ignore_ssize(write(error_pipe.write, &error, sizeof(error)));
            exit(1);
        }

        if (chdir(current_path_zero_terminated.data) < 0)
        {
            int const error = errno;
            ignore_ssize(write(error_pipe.write, &error, sizeof(error)));
            exit(1);
        }

        // close inherited file descriptors
        long max_fd = sysconf(_SC_OPEN_MAX);
        for (int i = 3; i < max_fd; ++i)
        {
            if (i == error_pipe.write)
            {
                continue;
            }
            close(i); // ignore errors because we will close many non-file-descriptors
        }

#ifdef __linux__
        // kill the child when the parent exits
        if (prctl(PR_SET_PDEATHSIG, SIGHUP) < 0)
        {
            int const error = errno;
            ignore_ssize(write(error_pipe.write, &error, sizeof(error)));
            exit(1);
        }
#endif

        execvp(executable_zero_terminated.data, exec_arguments);
        int const error = errno;
        ignore_ssize(write(error_pipe.write, &error, sizeof(error)));
        exit(1);
    }
    close(error_pipe.write);
    int child_error = 0;
    // result if read is irrelevant here
    ignore_ssize(read(error_pipe.read, &child_error, sizeof(child_error)));
    close(error_pipe.read);
    unicode_string_free(&current_path_zero_terminated);
    for (size_t i = 0; i <= argument_count; ++i)
    {
        deallocate(exec_arguments[i]);
    }
    deallocate(exec_arguments);
    if ((forked < 0) || (child_error != 0))
    {
        child_process const created = {-1};
        return create_process_result_create(success_no, created);
    }
    else
    {
        child_process const created = {forked};
        return create_process_result_create(success_yes, created);
    }
#endif
}

int wait_for_process_exit(child_process const process)
{
#ifdef _WIN32
    WaitForSingleObject(process.handle, INFINITE);
    DWORD exit_code = 1;
    if (!GetExitCodeProcess(process.handle, &exit_code))
    {
        LPG_TO_DO();
    }
    CloseHandle(process.handle);
    return (int)exit_code;
#else
    int status = 0;
    int const rc = waitpid(process.pid, &status, 0);
    if (rc == -1)
    {
        return -1;
    }
    if (WIFEXITED(status))
    {
        return WEXITSTATUS(status);
    }
    else if (WIFSIGNALED(status))
    {
        return -1;
    }
    else if (WIFSTOPPED(status))
    {
        return -1;
    }
    else if (WIFCONTINUED(status))
    {
        return -1;
    }
    else
    {
        LPG_UNREACHABLE();
    }
#endif
}
