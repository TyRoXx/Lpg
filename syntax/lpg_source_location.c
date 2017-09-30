#include "lpg_parse_expression.h"

source_location source_location_create(line_number line,
                                       column_number approximate_column)
{
    source_location const result = {line, approximate_column};
    return result;
}

bool source_location_equals(source_location left, source_location right)
{
    return (left.line == right.line) &&
           (left.approximate_column == right.approximate_column);
}
