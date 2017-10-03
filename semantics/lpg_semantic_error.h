#pragma once
#include "lpg_source_location.h"

typedef enum semantic_error_type
{
    semantic_error_unknown_element,
    semantic_error_expected_compile_time_type,
    semantic_error_no_members_on_enum_elements,
    semantic_error_type_mismatch,
    semantic_error_missing_argument,
    semantic_error_extraneous_argument,
    semantic_error_break_outside_of_loop,
    semantic_error_declaration_with_existing_name,
    semantic_error_missing_match_case,
    semantic_error_duplicate_match_case
} semantic_error_type;

typedef struct semantic_error
{
    semantic_error_type type;
    source_location where;
} semantic_error;

semantic_error semantic_error_create(semantic_error_type type, source_location where);
bool semantic_error_equals(semantic_error const left, semantic_error const right);
