#pragma once

#include <stdbool.h>
#include "lpg_try.h"

typedef struct lpg_thread_impl *lpg_thread;

typedef struct create_thread_result
{
    success_indicator is_success;
    lpg_thread success;
} create_thread_result;

typedef void (*thread_function)(void *);

create_thread_result create_thread(thread_function body, void *user);
void join_thread(lpg_thread joined);
