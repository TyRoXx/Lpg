#include <stdlib.h>
#include "lpg_string_literal.h"

decode_string_literal_result decode_string_literal(unicode_view source,
                                                   stream_writer decoded)
{
    bool escaped = false;
    for (size_t i = 1; i < source.length; i++)
    {
        char c = source.begin[i];
        if (c == '\n' || c == '\r')
        {
            return decode_string_literal_result_create(false, i);
        }
        if (escaped)
        {
            if (c == '"' || c == '\'' || c == '\\')
            {
                stream_writer_write_bytes(decoded, &c, 1);
            }
            else if (c == 'n' || c == 'r')
            {
                stream_writer_write_string(decoded, "\n");
            }
            else if (c == 't')
            {
                stream_writer_write_string(decoded, "\t");
            }
            else
            {
                return decode_string_literal_result_create(false, i);
            }
            escaped = false;
        }
        else if (c == '\\')
        {
            escaped = true;
        }
        else if (c == '"')
        {
            return decode_string_literal_result_create(true, i + 1);
        }
        else
        {
            stream_writer_write_bytes(decoded, &c, 1);
        }
    }
    return decode_string_literal_result_create(false, source.length);
}

decode_string_literal_result decode_string_literal_result_create(bool is_valid,
                                                                 size_t length)
{
    decode_string_literal_result result = {is_valid, length};
    return result;
}