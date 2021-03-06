#pragma once

#ifdef _WIN32
#include <Windows.h>
#endif
#include "lpg_try.h"
#include "lpg_unicode_view.h"

#ifdef _WIN32
typedef HANDLE file_handle;
#else
typedef int file_handle;
#endif

file_handle get_standard_input(void);
file_handle get_standard_output(void);
file_handle get_standard_error(void);

typedef struct child_process
{
#ifdef _WIN32
    HANDLE handle;
#else
    int pid;
#endif
} child_process;

typedef
#ifdef _WIN32
    DWORD
#else
    int
#endif
        error_code;

typedef struct create_process_result
{
    success_indicator success;
    union
    {
        child_process created;
        error_code error;
    };
} create_process_result;

create_process_result create_process_result_create_success(child_process created);
create_process_result create_process_result_create_error(error_code error);
create_process_result create_process(unicode_view const executable, unicode_view const *const arguments,
                                     size_t const argument_count, unicode_view const current_path,
                                     file_handle const input, file_handle const output, file_handle const error);
int wait_for_process_exit(child_process const process);
