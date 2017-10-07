#include "lpg_stream_writer.h"
#include "lpg_allocate.h"
#include <string.h>

success_indicator stream_writer_write_unicode_view(stream_writer writer, unicode_view string)
{
    return stream_writer_write_bytes(writer, string.begin, string.length);
}

success_indicator stream_writer_write_string(stream_writer writer, char const *c_str)
{
    return writer.write(writer.user, c_str, strlen(c_str));
}

success_indicator stream_writer_write_bytes(stream_writer writer, char const *data, size_t size)
{
    return writer.write(writer.user, data, size);
}

success_indicator stream_writer_write_integer(stream_writer writer, integer const value)
{
    char buffer[64];
    char const *const formatted = integer_format(value, lower_case_digits, 10, buffer, sizeof(buffer));
    return stream_writer_write_bytes(writer, formatted, (size_t)((buffer + sizeof(buffer)) - formatted));
}

void memory_writer_free(memory_writer *writer)
{
    if (writer->data)
    {
        deallocate(writer->data);
    }
}

success_indicator memory_writer_write(void *user, char const *data, size_t length)
{
    memory_writer *writer = user;
    size_t const new_used = (writer->used + length);
    if (new_used > writer->reserved)
    {
        size_t new_reserved = (writer->used * 2);
        if (new_reserved < new_used)
        {
            new_reserved = new_used;
        }
        writer->data = reallocate(writer->data, new_reserved);
        writer->reserved = new_reserved;
    }
    if (length > 0)
    {
        memmove(writer->data + writer->used, data, length);
    }
    writer->used = new_used;
    return success;
}

bool memory_writer_equals(memory_writer const writer, char const *c_str)
{
    size_t const length = strlen(c_str);
    if (length != writer.used)
    {
        return 0;
    }
    if (length == 0)
    {
        /*must not call memcmp with NULL*/
        return true;
    }
    return !memcmp(c_str, writer.data, length);
}

stream_writer memory_writer_erase(memory_writer *writer)
{
    stream_writer const result = {memory_writer_write, writer};
    return result;
}

unicode_view memory_writer_content(memory_writer const writer)
{
    return unicode_view_create(writer.data, writer.used);
}
