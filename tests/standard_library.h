#pragma once
#include "lpg_structure_member.h"

typedef struct standard_library_description
{
    structure globals;
    type *boolean;
} standard_library_description;

standard_library_description describe_standard_library(void);
void standard_library_description_free(
    standard_library_description const *value);
