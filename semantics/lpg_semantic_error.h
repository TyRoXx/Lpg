#pragma once
#include "lpg_source_location.h"
#include "lpg_unicode_view.h"

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
    semantic_error_duplicate_match_case,
    semantic_error_expected_interface,
    semantic_error_duplicate_impl,
    semantic_error_cannot_capture_runtime_variable,
    semantic_error_not_callable,
    semantic_error_duplicate_method_name,
    semantic_error_expected_structure,
    semantic_error_match_unsupported,
    semantic_error_duplicate_enum_element,
    semantic_error_expected_generic_type,
    semantic_error_expected_compile_time_value,
    semantic_error_import_failed
} semantic_error_type;

typedef struct semantic_error
{
    semantic_error_type type;
    source_location where;
} semantic_error;

semantic_error semantic_error_create(semantic_error_type type, source_location where);
bool semantic_error_equals(semantic_error const left, semantic_error const right);

typedef struct complete_semantic_error
{
    semantic_error relative;
    unicode_view file_name;
    unicode_view source;
} complete_semantic_error;

complete_semantic_error complete_semantic_error_create(semantic_error relative, unicode_view file_name,
                                                       unicode_view source);
bool complete_semantic_error_equals(complete_semantic_error const left, complete_semantic_error const right);
