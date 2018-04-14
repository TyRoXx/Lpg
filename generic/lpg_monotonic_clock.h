#pragma once
#include <stdint.h>

typedef struct duration
{
    uint64_t milliseconds;
} duration;

duration read_monotonic_clock(void);
duration duration_from_milliseconds(uint64_t milliseconds);
duration absolute_duration_difference(duration first, duration second);
