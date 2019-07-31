#pragma once

#include "lpg_unicode_view.h"

typedef struct source_file_lines
{
    size_t const *line_offsets;
    size_t line_count;
} source_file_lines;

source_file_lines source_file_lines_create(size_t const *line_offsets, size_t line_count);
bool source_file_lines_equals(source_file_lines const left, source_file_lines const right);

typedef struct source_file_lines_owning
{
    size_t *line_offsets;
    size_t line_count;
} source_file_lines_owning;

source_file_lines_owning source_file_lines_owning_create(size_t *line_offsets, size_t line_count);
void source_file_lines_owning_free(source_file_lines_owning const freed);
bool source_file_lines_owning_equals(source_file_lines_owning const left, source_file_lines_owning const right);
source_file_lines source_file_lines_from_owning(source_file_lines_owning const owning);
source_file_lines_owning source_file_lines_copy(source_file_lines const original);
source_file_lines_owning source_file_lines_owning_scan(unicode_view const source);

typedef struct source_file
{
    unicode_view name;
    unicode_view content;
    source_file_lines lines;
} source_file;

source_file source_file_create(unicode_view name, unicode_view content, source_file_lines lines);
bool source_file_equals(source_file const left, source_file const right);

typedef struct source_file_owning
{
    unicode_string name;
    unicode_string content;
    source_file_lines_owning lines;
} source_file_owning;

source_file_owning source_file_owning_create(unicode_string name, unicode_string content,
                                             source_file_lines_owning lines);
void source_file_owning_free(source_file_owning const freed);

source_file_owning source_file_to_owning(source_file const from);
source_file source_file_from_owning(source_file_owning const from);
