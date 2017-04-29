#include "lpg_parse_expression.h"
#include "lpg_assert.h"
#include "lpg_allocate.h"
#include "lpg_for.h"

source_location source_location_create(line_number line,
                                       column_number approximate_column)
{
    source_location result = {line, approximate_column};
    return result;
}

int source_location_equals(source_location left, source_location right)
{
    return (left.line == right.line) &&
           (left.approximate_column == right.approximate_column);
}
