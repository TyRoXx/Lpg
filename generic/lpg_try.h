#pragma once

typedef enum success_indicator
{
    success,
    failure
} success_indicator;

#define LPG_TRY(expression)                                                    \
    switch (expression)                                                        \
    {                                                                          \
    case failure:                                                              \
        return failure;                                                        \
    case success:                                                              \
        break;                                                                 \
    }

#define LPG_TRY_GOTO(expression, on_failure)                                   \
    switch (expression)                                                        \
    {                                                                          \
    case failure:                                                              \
        goto on_failure;                                                       \
    case success:                                                              \
        break;                                                                 \
    }
