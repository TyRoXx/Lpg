#include "lpg_source_file.h"
#include "lpg_allocate.h"
#include <stdlib.h>

source_file_lines source_file_lines_create(size_t const *line_offsets, size_t line_count)
{
    source_file_lines const result = {line_offsets, line_count};
    return result;
}

bool source_file_lines_equals(source_file_lines const left, source_file_lines const right)
{
    if (left.line_count != right.line_count)
    {
        return false;
    }
    for (size_t i = 0; i < left.line_count; ++i)
    {
        if (left.line_offsets[i] != right.line_offsets[i])
        {
            return false;
        }
    }
    return true;
}

source_file_lines_owning source_file_lines_owning_create(size_t *line_offsets, size_t line_count)
{
    source_file_lines_owning const result = {line_offsets, line_count};
    return result;
}

void source_file_lines_owning_free(source_file_lines_owning const freed)
{
    if (freed.line_offsets)
    {
        deallocate(freed.line_offsets);
    }
}

bool source_file_lines_owning_equals(source_file_lines_owning const left, source_file_lines_owning const right)
{
    if (left.line_count != right.line_count)
    {
        return false;
    }
    for (size_t i = 0; i < left.line_count; ++i)
    {
        if (left.line_offsets[i] != right.line_offsets[i])
        {
            return false;
        }
    }
    return true;
}

source_file_lines source_file_lines_from_owning(source_file_lines_owning const owning)
{
    return source_file_lines_create(owning.line_offsets, owning.line_count);
}

source_file_lines_owning source_file_lines_copy(source_file_lines const original)
{
    size_t *const line_offsets = copy_array_impl(original.line_offsets, sizeof(*line_offsets) * original.line_count);
    return source_file_lines_owning_create(line_offsets, original.line_count);
}

source_file_lines_owning source_file_lines_owning_scan(unicode_view const source)
{
    size_t *line_offsets = allocate(sizeof(*line_offsets));
    line_offsets[0] = 0;
    size_t line_count = 1;
    size_t line_offsets_allocated = 1;
    for (size_t i = 0; i < source.length; ++i)
    {
        char const c = source.begin[i];
        if (c == '\r')
        {
            if (((i + 1) < source.length) && (source.begin[i + 1] == '\n'))
            {
                // treat \r\n as one line break
                i += 1;
            }
            // count line
        }
        else if (c == '\n')
        {
            // count line
        }
        else
        {
            continue;
        }
        line_offsets = reallocate_array_exponentially(
            line_offsets, (line_count + 1), sizeof(*line_offsets), line_count, &line_offsets_allocated);
        line_offsets[line_count] = (i + 1u);
        line_count += 1u;
    }
    return source_file_lines_owning_create(line_offsets, line_count);
}

source_file source_file_create(unicode_view name, unicode_view content, source_file_lines lines)
{
    source_file const result = {name, content, lines};
    return result;
}

bool source_file_equals(source_file const left, source_file const right)
{
    return unicode_view_equals(left.name, right.name) && unicode_view_equals(left.content, right.content) &&
           source_file_lines_equals(left.lines, right.lines);
}

source_file_owning source_file_owning_create(unicode_string name, unicode_string content,
                                             source_file_lines_owning lines)
{
    source_file_owning const result = {name, content, lines};
    return result;
}

void source_file_owning_free(source_file_owning const freed)
{
    unicode_string_free(&freed.name);
    unicode_string_free(&freed.content);
    source_file_lines_owning_free(freed.lines);
}

source_file_owning source_file_to_owning(source_file const from)
{
    return source_file_owning_create(
        unicode_view_copy(from.name), unicode_view_copy(from.content), source_file_lines_copy(from.lines));
}

source_file source_file_from_owning(source_file_owning const from)
{
    return source_file_create(unicode_view_from_string(from.name), unicode_view_from_string(from.content),
                              source_file_lines_from_owning(from.lines));
}
