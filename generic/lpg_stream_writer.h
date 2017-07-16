#pragma once

#include "lpg_try.h"
#include "lpg_unicode_string.h"
#include <stdbool.h>
#include "lpg_unicode_view.h"
#include "lpg_non_null.h"

typedef struct stream_writer
{
    success_indicator (*write)(void *user, char const *data, size_t length);
    void *user;
} stream_writer;

success_indicator stream_writer_write_unicode_view(stream_writer writer,
                                                   unicode_view string);

success_indicator stream_writer_write_string(stream_writer writer,
                                             LPG_NON_NULL(char const *c_str));
success_indicator stream_writer_write_bytes(stream_writer writer,
                                            char const *data, size_t size);

typedef struct memory_writer
{
    char *data;
    size_t reserved;
    size_t used;
} memory_writer;

void memory_writer_free(LPG_NON_NULL(memory_writer *writer));
success_indicator memory_writer_write(void *user, char const *data,
                                      size_t length);
bool memory_writer_equals(memory_writer const writer,
                          LPG_NON_NULL(char const *c_str));
stream_writer memory_writer_erase(LPG_NON_NULL(memory_writer *writer));
unicode_view memory_writer_content(memory_writer const writer);
