#pragma once

#include "lpg_unicode_view.h"

typedef struct source_file
{
    unicode_view name;
    unicode_view content;
} source_file;

source_file source_file_create(unicode_view name, unicode_view content);
bool source_file_equals(source_file const left, source_file const right);

typedef struct source_file_owning
{
    unicode_string name;
    unicode_string content;
} source_file_owning;

source_file_owning source_file_owning_create(unicode_string name, unicode_string content);
void source_file_owning_free(source_file_owning const freed);

source_file_owning source_file_to_owning(source_file const from);
source_file source_file_from_owning(source_file_owning const from);
