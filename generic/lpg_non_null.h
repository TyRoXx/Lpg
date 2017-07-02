#pragma once

#ifdef __clang__
#define LPG_NON_NULL(parameter) parameter __attribute__((nonnull))
#else
// GCC does not support the attribute for no reason
#define LPG_NON_NULL(parameter) parameter
#endif
