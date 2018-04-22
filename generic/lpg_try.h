#pragma once

typedef enum success_indicator
{
    success_yes,
    success_no
} success_indicator;

#define LPG_TRY(expression)                                                                                            \
    switch (expression)                                                                                                \
    {                                                                                                                  \
    case success_no:                                                                                                   \
        return success_no;                                                                                             \
    case success_yes:                                                                                                  \
        break;                                                                                                         \
    }

#define LPG_TRY_GOTO(expression, on_failure)                                                                           \
    switch (expression)                                                                                                \
    {                                                                                                                  \
    case success_no:                                                                                                   \
        goto on_failure;                                                                                               \
    case success_yes:                                                                                                  \
        break;                                                                                                         \
    }
