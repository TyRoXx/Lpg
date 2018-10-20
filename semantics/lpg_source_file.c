#include "lpg_source_file.h"

source_file source_file_create(unicode_view name, unicode_view content)
{
    source_file const result = {name, content};
    return result;
}

bool source_file_equals(source_file const left, source_file const right)
{
    return unicode_view_equals(left.name, right.name) && unicode_view_equals(left.content, right.content);
}

source_file_owning source_file_owning_create(unicode_string name, unicode_string content)
{
    source_file_owning const result = {name, content};
    return result;
}

void source_file_owning_free(source_file_owning const freed)
{
    unicode_string_free(&freed.name);
    unicode_string_free(&freed.content);
}

source_file_owning source_file_to_owning(source_file const from)
{
    return source_file_owning_create(unicode_view_copy(from.name), unicode_view_copy(from.content));
}

source_file source_file_from_owning(source_file_owning const from)
{
    return source_file_create(unicode_view_from_string(from.name), unicode_view_from_string(from.content));
}
