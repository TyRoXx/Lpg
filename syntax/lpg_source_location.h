#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef size_t line_number;
typedef size_t column_number;

typedef struct source_location
{
    line_number line;
    column_number approximate_column;
} source_location;

source_location source_location_create(line_number line, column_number approximate_column);
bool source_location_equals(source_location left, source_location right);
