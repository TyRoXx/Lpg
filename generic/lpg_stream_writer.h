#pragma once

#include "lpg_try.h"
#include "lpg_unicode_string.h"
#include <stdbool.h>
#include "lpg_unicode_view.h"
#include "lpg_non_null.h"
#include "lpg_use_result.h"
#include "lpg_integer.h"

typedef struct stream_writer
{
    success_indicator (*write)(void *user, char const *data, size_t length);
    void *user;
} stream_writer;

success_indicator stream_writer_write_unicode_view(stream_writer writer, unicode_view string) LPG_USE_RESULT;

static inline success_indicator stream_writer_write_string(stream_writer writer, char const *c_str)
{
    return writer.write(writer.user, c_str, strlen(c_str));
}

success_indicator stream_writer_write_bytes(stream_writer writer, char const *data, size_t size) LPG_USE_RESULT;
success_indicator stream_writer_write_integer(stream_writer writer, integer const value) LPG_USE_RESULT;

typedef struct memory_writer
{
    char *data;
    size_t reserved;
    size_t used;
} memory_writer;

void memory_writer_free(LPG_NON_NULL(memory_writer *writer));
success_indicator memory_writer_write(void *user, char const *data, size_t length) LPG_USE_RESULT;
bool memory_writer_equals(memory_writer const writer, LPG_NON_NULL(char const *c_str)) LPG_USE_RESULT;
stream_writer memory_writer_erase(LPG_NON_NULL(memory_writer *writer)) LPG_USE_RESULT;
unicode_view memory_writer_content(memory_writer const writer) LPG_USE_RESULT;
