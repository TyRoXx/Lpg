#pragma once
#include "lpg_expression.h"
#include "lpg_expression_parser.h"
#include "lpg_source_location.h"
#include "lpg_tokenize.h"
#include "lpg_unicode_view.h"

bool is_end_of_file(LPG_NON_NULL(rich_token const *token));

typedef struct expression_parser_result
{
    bool is_success;
    expression success;
} expression_parser_result;

expression_parser_result parse_expression(LPG_NON_NULL(expression_parser *const parser), size_t const indentation,
                                          bool const may_be_statement);

sequence parse_program(LPG_NON_NULL(expression_parser *parser));
