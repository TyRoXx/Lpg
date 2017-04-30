#include "lpg_stream_writer.h"
#include "lpg_allocate.h"
#include <string.h>

success_indicator stream_writer_write_string(stream_writer writer,
                                             char const *c_str)
{
    return writer.write(writer.user, c_str, strlen(c_str));
}

success_indicator stream_writer_write_bytes(stream_writer writer,
                                            char const *data, size_t size)
{
    return writer.write(writer.user, data, size);
}

void memory_writer_free(memory_writer *writer)
{
    deallocate(writer->data);
}

success_indicator memory_writer_write(void *user, char const *data,
                                      size_t length)
{
    memory_writer *writer = user;
    size_t new_used = (writer->used + length);
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
    memmove(writer->data + writer->used, data, length);
    writer->used = new_used;
    return success;
}

bool memory_writer_equals(memory_writer const writer, char const *c_str)
{
    size_t length = strlen(c_str);
    if (length != writer.used)
    {
        return 0;
    }
    return !memcmp(c_str, writer.data, length);
}

stream_writer memory_writer_erase(memory_writer *writer)
{
    stream_writer result = {memory_writer_write, writer};
    return result;
}
