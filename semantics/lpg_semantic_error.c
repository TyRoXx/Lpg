#include "lpg_semantic_error.h"

semantic_error semantic_error_create(semantic_error_type type, source_location where)
{
    semantic_error const result = {type, where};
    return result;
}

bool semantic_error_equals(semantic_error const left, semantic_error const right)
{
    return (left.type == right.type) && source_location_equals(left.where, right.where);
}

complete_semantic_error complete_semantic_error_create(semantic_error relative, unicode_view file_name,
                                                       unicode_view source)
{
    complete_semantic_error const result = {relative, file_name, source};
    return result;
}

bool complete_semantic_error_equals(complete_semantic_error const left, complete_semantic_error const right)
{
    return semantic_error_equals(left.relative, right.relative) &&
           unicode_view_equals(left.file_name, right.file_name) && unicode_view_equals(left.source, right.source);
}
