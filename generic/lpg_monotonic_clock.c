#include "lpg_monotonic_clock.h"

#ifdef _WIN32
#include <Windows.h>

duration read_monotonic_clock(void)
{
    return duration_from_milliseconds(GetTickCount64());
}
#else
#include <time.h>
#include "lpg_assert.h"

duration read_monotonic_clock(void)
{
    struct timespec result;
    ASSERT(clock_gettime(CLOCK_MONOTONIC, &result) == 0);
    return duration_from_milliseconds(((uint64_t)result.tv_sec * 1000u) + ((uint64_t)result.tv_nsec / 1000u / 1000u));
}
#endif

duration duration_from_milliseconds(uint64_t milliseconds)
{
    duration const result = {milliseconds};
    return result;
}

duration absolute_duration_difference(duration first, duration second)
{
    if (first.milliseconds < second.milliseconds)
    {
        return duration_from_milliseconds(second.milliseconds - first.milliseconds);
    }
    return duration_from_milliseconds(first.milliseconds - second.milliseconds);
}
