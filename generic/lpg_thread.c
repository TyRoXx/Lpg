#include "lpg_thread.h"
#include "lpg_allocate.h"
#include "lpg_assert.h"
#ifdef _WIN32
#include <Windows.h>
#include <handleapi.h>
#include <process.h>

typedef struct lpg_thread_impl
{
    uintptr_t handle;
    thread_function actual_body;
    void *user;
} lpg_thread_impl;

static unsigned __stdcall win32_thread_function(void *argument)
{
    lpg_thread_impl *const impl = argument;
    impl->actual_body(impl->user);
    return 0;
}

create_thread_result create_thread(thread_function body, void *user)
{
    lpg_thread_impl *const impl = allocate(sizeof(*impl));
    impl->actual_body = body;
    impl->user = user;
    uintptr_t const rc = _beginthreadex(NULL, 0, win32_thread_function, impl, 0, NULL);
    if (rc == 0)
    {
        create_thread_result const result = {success_no, NULL};
        deallocate(impl);
        return result;
    }
    impl->handle = rc;
    create_thread_result const result = {success_yes, impl};
    return result;
}

void join_thread(lpg_thread joined)
{
    ASSUME(joined);
    ASSERT(WAIT_OBJECT_0 == WaitForSingleObject((HANDLE)joined->handle, INFINITE));
    ASSERT(CloseHandle((HANDLE)joined->handle));
    deallocate(joined);
}
#else
#include <pthread.h>

typedef struct lpg_thread_impl
{
    pthread_t handle;
    thread_function actual_body;
    void *user;
} lpg_thread_impl;

static void *posix_thread_function(void *argument)
{
    lpg_thread_impl *const impl = argument;
    impl->actual_body(impl->user);
    return 0;
}

create_thread_result create_thread(thread_function body, void *user)
{
    lpg_thread_impl *const impl = allocate(sizeof(*impl));
    impl->actual_body = body;
    impl->user = user;
    int const rc = pthread_create(&impl->handle, NULL, posix_thread_function, impl);
    if (rc != 0)
    {
        create_thread_result const result = {success_no, NULL};
        deallocate(impl);
        return result;
    }
    create_thread_result const result = {success_yes, impl};
    return result;
}

void join_thread(lpg_thread joined)
{
    ASSUME(joined);
    ASSERT(0 == pthread_join(joined->handle, NULL));
    deallocate(joined);
}
#endif
