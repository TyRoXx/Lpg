#pragma once

#ifdef _WIN32
#include <Windows.h>
#endif
#include "lpg_unicode_view.h"
#include "lpg_try.h"

#ifdef _WIN32
typedef HANDLE file_handle;
#else
typedef int file_handle;
#endif

typedef struct child_process
{
#ifdef _WIN32
    HANDLE handle;
#else
    int pid;
#endif
} child_process;

typedef struct create_process_result
{
    success_indicator success;
    child_process created;
} create_process_result;

create_process_result create_process_result_create(success_indicator success, child_process created);
create_process_result create_process(unicode_view const executable, unicode_view const *const arguments,
                                     size_t const argument_count, unicode_view const current_path,
                                     file_handle const input, file_handle const output, file_handle const error);
