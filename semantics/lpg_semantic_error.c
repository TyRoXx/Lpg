#include "lpg_semantic_error.h"

semantic_error semantic_error_create(semantic_error_type type,
                                     source_location where)
{
    semantic_error const result = {type, where};
    return result;
}

bool semantic_error_equals(semantic_error const left,
                           semantic_error const right)
{
    return (left.type == right.type) &&
           source_location_equals(left.where, right.where);
}