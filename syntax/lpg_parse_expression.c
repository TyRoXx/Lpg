#include "lpg_parse_expression.h"
#include "lpg_assert.h"
#include "lpg_allocate.h"
#include "lpg_for.h"
#include <string.h>

rich_token rich_token_create(tokenize_status status, token_type token, unicode_view content, source_location where)
{
    rich_token const result = {status, token, content, where};
    return result;
}

bool is_end_of_file(rich_token const *token)
{
    return (token->status == tokenize_success) && (token->content.length == 0);
}

parse_error parse_error_create(parse_error_type type, source_location where)
{
    parse_error const result = {type, where};
    return result;
}

bool parse_error_equals(parse_error left, parse_error right)
{
    return (left.type == right.type) && source_location_equals(left.where, right.where);
}

expression_parser expression_parser_create(rich_token_producer find_next_token, callback_user find_next_token_user,
                                           parse_error_handler on_error, callback_user on_error_user)
{
    expression_parser const result = {
        find_next_token,
        find_next_token_user,
        on_error,
        on_error_user,
        {{tokenize_success, 0, {NULL, 0}, {0, 0}}, {tokenize_success, 0, {NULL, 0}, {0, 0}}},
        0};
    return result;
}

bool expression_parser_has_remaining_non_empty_tokens(expression_parser const *const parser)
{
    return (parser->cached_token_count > 0) && (parser->cached_tokens[0].content.length > 0);
}

static rich_token peek_at(expression_parser *parser, size_t const offset)
{
    ASSUME(offset < LPG_ARRAY_SIZE(parser->cached_tokens));
    while (parser->cached_token_count <= offset)
    {
        parser->cached_tokens[parser->cached_token_count] = parser->find_next_token(parser->find_next_token_user);
        switch (parser->cached_tokens[parser->cached_token_count].status)
        {
        case tokenize_success:
            parser->cached_token_count += 1;
            break;

        case tokenize_invalid:
            parser->on_error(
                parse_error_create(parse_error_invalid_token, parser->cached_tokens[parser->cached_token_count].where),
                parser->on_error_user);
            break;
        }
    }
    ASSUME(parser->cached_tokens[offset].status == tokenize_success);
    return parser->cached_tokens[offset];
}

static rich_token peek(expression_parser *parser)
{
    return peek_at(parser, 0);
}

static void pop_n(expression_parser *parser, size_t const count)
{
    ASSUME(parser->cached_token_count >= count);
    parser->cached_token_count -= count;
    memmove(parser->cached_tokens, parser->cached_tokens + count,
            (sizeof(*parser->cached_tokens) * parser->cached_token_count));
}

static void pop(expression_parser *parser)
{
    pop_n(parser, 1);
}

static bool is_same_indentation_level(rich_token const found, size_t const expected)
{
    return (found.token == token_indentation) && (found.content.length == (expected * spaces_for_indentation));
}

static bool line_is_empty(expression_parser *parser)
{
    rich_token head;
    size_t whitespace_token_count = 0;
    for (;;)
    {
        head = peek_at(parser, whitespace_token_count);
        if (head.status == tokenize_invalid)
        {
            return false;
        }
        if (head.content.length > 0 && (head.token == token_space || head.token == token_indentation))
        {
            whitespace_token_count++;
        }
        else
        {
            break;
        }
    }
    if (head.token == token_newline || head.content.length == 0)
    {
        pop_n(parser, whitespace_token_count + 1);
        return true;
    }
    return false;
}

static bool parse_space(expression_parser *const parser)
{
    rich_token maybe_space = peek(parser);

    if (maybe_space.token != token_space || maybe_space.content.length == 0)
    {
        parser->on_error(parse_error_create(parse_error_expected_space, maybe_space.where), parser->on_error_user);
        return false;
    }

    pop(parser);
    return true;
}

static void parse_optional_space(expression_parser *const parser)
{
    rich_token maybe_space = peek(parser);

    if (maybe_space.token == token_space && maybe_space.content.length > 0)
    {
        pop(parser);
    }
}

static sequence parse_sequence(expression_parser *parser, size_t indentation)
{
    expression *elements = NULL;
    size_t element_count = 0;
    for (;;)
    {
        rich_token const indentation_token = peek(parser);
        if (is_end_of_file(&indentation_token))
        {
            break;
        }
        if ((indentation == 0) || is_same_indentation_level(indentation_token, indentation))
        {
            if (indentation > 0)
            {
                pop(parser);
            }

            if (line_is_empty(parser))
            {
                continue;
            }
            expression_parser_result const element = parse_expression(parser, indentation, 1);
            if (element.is_success)
            {
                /*TODO: avoid O(N^2)*/
                elements = reallocate_array(elements, (element_count + 1), sizeof(*elements));
                elements[element_count] = element.success;
                ++element_count;
            }
            line_is_empty(parser);
        }
        else
        {
            break;
        }
    }
    return sequence_create(elements, element_count);
}

static expression_parser_result const expression_parser_result_failure = {
    0, {expression_type_lambda, {{{NULL, 0, NULL}, NULL, {0, 0}}}}};

static expression_parser_result parse_tuple(expression_parser *parser, size_t indentation,
                                            source_location const opening_brace);

static expression_parser_result parse_returnable(expression_parser *const parser, size_t const indentation,
                                                 bool const may_be_binary);

static expression_parser_result parse_loop(expression_parser *parser, size_t indentation)
{
    rich_token const maybe_newline = peek(parser);
    pop(parser);
    if (maybe_newline.token != token_newline)
    {
        parser->on_error(parse_error_create(parse_error_expected_newline, maybe_newline.where), parser->on_error_user);
        return expression_parser_result_failure;
    }
    expression_parser_result const result = {1, expression_from_loop(parse_sequence(parser, (indentation + 1)))};
    return result;
}

static int parse_match_cases(expression_parser *parser, size_t const indentation, match_case **cases,
                             size_t *case_count)
{
    for (;;)
    {
        rich_token const indentation_token = peek(parser);
        if ((indentation_token.token != token_indentation) ||
            (indentation_token.content.length != (indentation * spaces_for_indentation)))
        {
            return 1;
        }
        pop(parser);
        rich_token const expected_case = peek(parser);
        if (expected_case.token != token_case)
        {
            parser->on_error(parse_error_create(parse_error_expected_case, expected_case.where), parser->on_error_user);
            return 0;
        }
        pop(parser);
        if (!parse_space(parser))
        {
            return 0;
        }
        expression_parser_result const key = parse_expression(parser, indentation, 0);
        if (!key.is_success)
        {
            return 0;
        }
        {
            rich_token const colon = peek(parser);
            pop(parser);
            if (colon.token != token_colon)
            {
                parser->on_error(parse_error_create(parse_error_expected_colon, colon.where), parser->on_error_user);
                return 0;
            }
        }
        rich_token const space = peek(parser);
        if (space.token == token_newline)
        {
            pop(parser);
            sequence const value = parse_sequence(parser, (indentation + 1));
            if (value.length < 1)
            {
                parser->on_error(
                    parse_error_create(parse_error_expected_expression, peek(parser).where), parser->on_error_user);
                return 0;
            }
            *cases = reallocate_array(*cases, (*case_count + 1), sizeof(**cases));
            (*cases)[*case_count] = match_case_create(
                expression_allocate(key.success), expression_allocate(expression_from_sequence(value)));
        }
        else
        {
            parse_optional_space(parser);

            expression_parser_result const value = parse_expression(parser, indentation, 0);
            if (!value.is_success)
            {
                return 0;
            }
            {
                rich_token const newline = peek(parser);
                pop(parser);
                if (newline.token != token_newline)
                {
                    parser->on_error(
                        parse_error_create(parse_error_expected_newline, newline.where), parser->on_error_user);
                    return 0;
                }
            }
            *cases = reallocate_array(*cases, (*case_count + 1), sizeof(**cases));
            (*cases)[*case_count] =
                match_case_create(expression_allocate(key.success), expression_allocate(value.success));
        }
        ++(*case_count);
    }
}

static expression_parser_result parse_match(expression_parser *parser, size_t const indentation,
                                            source_location const begin)
{
    if (!parse_space(parser))
    {
        return expression_parser_result_failure;
    }
    expression_parser_result const input = parse_expression(parser, indentation, 0);
    if (!input.is_success)
    {
        return input;
    }
    {
        rich_token const newline = peek(parser);
        pop(parser);
        if (newline.token != token_newline)
        {
            parser->on_error(parse_error_create(parse_error_expected_newline, newline.where), parser->on_error_user);
            expression_free(&input.success);
            return expression_parser_result_failure;
        }
    }
    match_case *cases = NULL;
    size_t case_count = 0;
    if (parse_match_cases(parser, (indentation + 1), &cases, &case_count))
    {
        expression_parser_result const result = {
            1, expression_from_match(match_create(begin, expression_allocate(input.success), cases, case_count))};
        return result;
    }
    expression_free(&input.success);
    LPG_FOR(size_t, i, case_count)
    {
        match_case_free(cases + i);
    }
    if (cases)
    {
        deallocate(cases);
    }
    return expression_parser_result_failure;
}

static void clean_up_parameters(parameter *const parameters, size_t parameter_count)
{
    if (parameters)
    {
        for (size_t i = 0; i < parameter_count; ++i)
        {
            parameter_free(parameters + i);
        }
        deallocate(parameters);
    }
}

static expression_parser_result parse_lambda_body(expression_parser *const parser, size_t const indentation,
                                                  function_header_tree const header, source_location const lambda_begin)
{
    rich_token const next_token = peek(parser);
    if (next_token.token == token_space && !header.return_type)
    {
        pop(parser);
        expression_parser_result const body = parse_expression(parser, indentation, false);
        if (!body.is_success)
        {
            return expression_parser_result_failure;
        }
        expression_parser_result const result = {
            1, expression_from_lambda(lambda_create(header, expression_allocate(body.success), lambda_begin))};

        return result;
    }
    if (next_token.token == token_newline)
    {
        pop(parser);
        sequence const body = parse_sequence(parser, (indentation + 1));
        expression_parser_result const result = {
            1, expression_from_lambda(
                   lambda_create(header, expression_allocate(expression_from_sequence(body)), lambda_begin))};
        return result;
    }
    pop(parser);
    parser->on_error(parse_error_create(parse_error_expected_lambda_body, next_token.where), parser->on_error_user);

    clean_up_parameters(header.parameters, header.parameter_count);
    if (header.return_type != NULL)
    {
        expression_deallocate(header.return_type);
    }

    return expression_parser_result_failure;
}

typedef struct function_header_parse_result
{
    bool is_success;
    function_header_tree header;
} function_header_parse_result;

static function_header_parse_result const function_header_parse_result_failure = {false, {NULL, 0, NULL}};

static function_header_parse_result parse_function_header(expression_parser *const parser, size_t const indentation)
{
    parameter *parameters = NULL;
    size_t parameter_count = 0;
    expression *result_type = NULL;
    for (;;)
    {
        rich_token const head = peek(parser);
        if (head.token == token_right_parenthesis)
        {
            pop(parser);
            if ((peek(parser).token == token_colon))
            {
                pop(parser);
                if (peek(parser).token != token_space)
                {
                    LPG_TO_DO();
                }
                else
                {
                    pop(parser);
                }
                expression_parser_result const type = parse_returnable(parser, indentation, false);
                if (!type.is_success)
                {
                    return function_header_parse_result_failure;
                }
                result_type = expression_allocate(type.success);
            }
            break;
        }
        if (parameter_count >= 1)
        {
            if (head.token == token_comma)
            {
                pop(parser);
                parse_optional_space(parser);
            }
            else
            {
                pop(parser);
                parser->on_error(parse_error_create(parse_error_expected_comma, head.where), parser->on_error_user);
                clean_up_parameters(parameters, parameter_count);
                return function_header_parse_result_failure;
            }
        }

        rich_token const name = peek(parser);
        pop(parser);
        if (name.token != token_identifier)
        {
            parser->on_error(parse_error_create(parse_error_expected_identifier, name.where), parser->on_error_user);
            clean_up_parameters(parameters, parameter_count);
            return function_header_parse_result_failure;
        }

        {
            rich_token const colon = peek(parser);
            pop(parser);
            if (colon.token != token_colon)
            {
                parser->on_error(parse_error_create(parse_error_expected_colon, colon.where), parser->on_error_user);
                clean_up_parameters(parameters, parameter_count);
                return function_header_parse_result_failure;
            }
        }

        if (!parse_space(parser))
        {
            clean_up_parameters(parameters, parameter_count);
            return function_header_parse_result_failure;
        }

        expression_parser_result const parsed_type = parse_expression(parser, indentation, false);
        if (!parsed_type.is_success)
        {
            clean_up_parameters(parameters, parameter_count);
            return function_header_parse_result_failure;
        }

        parameters = reallocate_array(parameters, (parameter_count + 1), sizeof(*parameters));
        parameters[parameter_count] =
            parameter_create(identifier_expression_create(unicode_view_copy(name.content), name.where),
                             expression_allocate(parsed_type.success));
        ++parameter_count;
    }
    function_header_parse_result const result = {
        true, function_header_tree_create(parameters, parameter_count, result_type)};
    return result;
}

static expression_parser_result parse_lambda(expression_parser *const parser, size_t const indentation,
                                             source_location const lambda_begin)
{
    function_header_parse_result const parsed_header = parse_function_header(parser, indentation);
    if (!parsed_header.is_success)
    {
        return expression_parser_result_failure;
    }
    return parse_lambda_body(parser, indentation, parsed_header.header, lambda_begin);
}

static generic_parameter_list parse_generic_parameters(expression_parser *const parser)
{
    rich_token const maybe_bracket = peek(parser);
    if (maybe_bracket.token != token_left_bracket)
    {
        generic_parameter_list const result = {NULL, 0};
        return result;
    }
    pop(parser);
    unicode_string *generic_parameters = NULL;
    size_t generic_parameter_count = 0;
    for (;;)
    {
        rich_token const next = peek(parser);
        if (next.token == token_identifier)
        {
            generic_parameters =
                reallocate_array(generic_parameters, generic_parameter_count + 1, sizeof(*generic_parameters));
            generic_parameters[generic_parameter_count] = unicode_view_copy(next.content);
            ++generic_parameter_count;
            pop(parser);
            rich_token const after_parameter = peek(parser);
            if (after_parameter.token == token_comma)
            {
                pop(parser);
                parse_optional_space(parser);
            }
            else if (after_parameter.token != token_right_bracket)
            {
                parser->on_error(parse_error_create(parse_error_expected_right_bracket, after_parameter.where),
                                 parser->on_error_user);
                break;
            }
        }
        else if (next.token == token_right_bracket)
        {
            pop(parser);
            break;
        }
        else
        {
            parser->on_error(parse_error_create(parse_error_expected_right_bracket, next.where), parser->on_error_user);
            break;
        }
    }

    generic_parameter_list const result = {generic_parameters, generic_parameter_count};
    return result;
}

static expression_parser_result parse_interface(expression_parser *const parser, size_t const indentation,
                                                source_location const begin)
{
    generic_parameter_list const parameters = parse_generic_parameters(parser);

    interface_expression_method *methods = NULL;
    size_t method_count = 0;
    bool is_success = true;
    for (;;)
    {
        {
            rich_token const newline = peek_at(parser, 0);
            if (newline.token != token_newline)
            {
                break;
            }
        }
        {
            rich_token const indent = peek_at(parser, 1);
            if (!is_same_indentation_level(indent, indentation))
            {
                break;
            }
        }
        pop_n(parser, 2);
        rich_token const name = peek(parser);
        if (name.token != token_identifier)
        {
            parser->on_error(parse_error_create(parse_error_expected_identifier, name.where), parser->on_error_user);
            is_success = false;
            break;
        }
        pop(parser);

        {
            rich_token const left_parenthesis = peek(parser);
            if (left_parenthesis.token != token_left_parenthesis)
            {
                parser->on_error(parse_error_create(parse_error_expected_parameter_list, left_parenthesis.where),
                                 parser->on_error_user);
                is_success = false;
                break;
            }
            pop(parser);
        }

        function_header_parse_result const header_parsed = parse_function_header(parser, indentation);
        if (!header_parsed.is_success)
        {
            is_success = false;
            break;
        }

        methods = reallocate_array(methods, (method_count + 1), sizeof(*methods));
        methods[method_count] = interface_expression_method_create(
            identifier_expression_create(unicode_view_copy(name.content), name.where), header_parsed.header);
        ++method_count;
    }
    interface_expression const parsed = interface_expression_create(parameters, begin, methods, method_count);
    if (is_success)
    {
        expression_parser_result const result = {true, expression_from_interface(parsed)};
        return result;
    }
    interface_expression_free(parsed);
    return expression_parser_result_failure;
}

static expression_parser_result parse_struct(expression_parser *const parser, size_t const indentation,
                                             source_location const begin)
{
    struct_expression_element *elements = NULL;
    size_t element_count = 0;
    bool is_success = true;
    for (;;)
    {
        {
            rich_token const newline = peek_at(parser, 0);
            if (newline.token != token_newline)
            {
                break;
            }
        }
        {
            rich_token const indent = peek_at(parser, 1);
            if (!is_same_indentation_level(indent, indentation))
            {
                break;
            }
        }
        pop_n(parser, 2);
        rich_token const name = peek(parser);
        if (name.token != token_identifier)
        {
            parser->on_error(parse_error_create(parse_error_expected_identifier, name.where), parser->on_error_user);
            is_success = false;
            break;
        }
        pop(parser);

        {
            rich_token const colon = peek(parser);
            if (colon.token != token_colon)
            {
                parser->on_error(parse_error_create(parse_error_expected_colon, colon.where), parser->on_error_user);
                is_success = false;
                break;
            }
            pop(parser);
        }

        if (!parse_space(parser))
        {
            is_success = false;
            break;
        }

        expression_parser_result const type_parsed = parse_expression(parser, indentation, false);
        if (!type_parsed.is_success)
        {
            is_success = false;
            break;
        }

        elements = reallocate_array(elements, (element_count + 1), sizeof(*elements));
        elements[element_count] = struct_expression_element_create(
            identifier_expression_create(unicode_view_copy(name.content), name.where), type_parsed.success);
        ++element_count;
    }
    struct_expression const parsed = struct_expression_create(begin, elements, element_count);
    if (is_success)
    {
        expression_parser_result const result = {true, expression_from_struct(parsed)};
        return result;
    }
    struct_expression_free(&parsed);
    return expression_parser_result_failure;
}

static expression_parser_result parse_enum(expression_parser *const parser, size_t const indentation,
                                           source_location const begin)
{
    generic_parameter_list const parameters = parse_generic_parameters(parser);

    enum_expression_element *elements = NULL;
    enum_element_id element_count = 0;
    bool is_success = true;
    for (;;)
    {
        {
            rich_token const newline = peek_at(parser, 0);
            if (newline.token != token_newline)
            {
                break;
            }
        }
        {
            rich_token const indent = peek_at(parser, 1);
            if (!is_same_indentation_level(indent, indentation))
            {
                break;
            }
        }
        pop_n(parser, 2);
        rich_token const name = peek(parser);
        if (name.token != token_identifier)
        {
            parser->on_error(parse_error_create(parse_error_expected_identifier, name.where), parser->on_error_user);
            is_success = false;
            break;
        }
        pop(parser);

        expression *state = NULL;
        rich_token const left_parenthesis = peek(parser);
        if (left_parenthesis.token == token_left_parenthesis)
        {
            pop(parser);
            expression_parser_result const maybe_state = parse_expression(parser, indentation, false);
            if (!maybe_state.is_success)
            {
                is_success = false;
                break;
            }
            rich_token const right_parenthesis = peek(parser);
            if (right_parenthesis.token == token_right_parenthesis)
            {
                pop(parser);
            }
            else
            {
                parser->on_error(
                    parse_error_create(parse_error_expected_right_parenthesis, name.where), parser->on_error_user);
                is_success = false;
                break;
            }
            state = expression_allocate(maybe_state.success);
        }

        elements = reallocate_array(elements, (element_count + 1), sizeof(*elements));
        elements[element_count] = enum_expression_element_create(unicode_view_copy(name.content), state);
        ++element_count;
    }
    enum_expression const parsed = enum_expression_create(begin, parameters, elements, element_count);
    if (is_success)
    {
        expression_parser_result const result = {true, expression_from_enum(parsed)};
        return result;
    }
    enum_expression_free(parsed);
    return expression_parser_result_failure;
}

static expression_parser_result parse_type_of(expression_parser *const parser, size_t const indentation,
                                              source_location const begin)
{
    {
        rich_token const left_parenthesis = peek(parser);
        if (left_parenthesis.token != token_left_parenthesis)
        {
            parser->on_error(parse_error_create(parse_error_expected_left_parenthesis, left_parenthesis.where),
                             parser->on_error_user);
            return expression_parser_result_failure;
        }
    }
    pop(parser);
    expression_parser_result const target_parsed = parse_expression(parser, indentation, false);
    if (!target_parsed.is_success)
    {
        return expression_parser_result_failure;
    }
    {
        rich_token const right_parenthesis = peek(parser);
        if (right_parenthesis.token != token_right_parenthesis)
        {
            parser->on_error(parse_error_create(parse_error_expected_right_parenthesis, right_parenthesis.where),
                             parser->on_error_user);
            expression_free(&target_parsed.success);
            return expression_parser_result_failure;
        }
    }
    pop(parser);
    expression_parser_result const result = {
        true, expression_from_type_of(type_of_expression_create(begin, expression_allocate(target_parsed.success)))};
    return result;
}

static expression_parser_result parse_import(expression_parser *const parser, source_location const begin)
{
    if (!parse_space(parser))
    {
        return expression_parser_result_failure;
    }

    rich_token const name = peek(parser);
    if (name.token != token_identifier)
    {
        parser->on_error(parse_error_create(parse_error_expected_identifier, name.where), parser->on_error_user);
        return expression_parser_result_failure;
    }
    pop(parser);
    expression_parser_result const result = {
        true, expression_from_import(import_expression_create(
                  begin, identifier_expression_create(unicode_view_copy(name.content), name.where)))};
    return result;
}

static expression_parser_result parse_callable(expression_parser *parser, size_t indentation)
{
    for (;;)
    {
        rich_token const head = peek(parser);
        if (is_end_of_file(&head))
        {
            pop(parser);
            parser->on_error(parse_error_create(parse_error_expected_expression, head.where), parser->on_error_user);
            return expression_parser_result_failure;
        }
        switch (head.token)
        {
        case token_identifier:
        {
            pop(parser);
            expression_parser_result const result = {1, expression_from_identifier(identifier_expression_create(
                                                            unicode_view_copy(head.content), head.where))};
            return result;
        }

        case token_break:
        {
            pop(parser);
            expression_parser_result const result = {1, expression_from_break(head.where)};
            return result;
        }

        case token_loop:
            pop(parser);
            return parse_loop(parser, indentation);

        case token_match:
            pop(parser);
            return parse_match(parser, indentation, head.where);

        case token_left_parenthesis:
            pop(parser);
            return parse_lambda(parser, indentation, head.where);

        case token_left_curly_brace:
        {
            pop(parser);
            return parse_tuple(parser, indentation, head.where);
        }

        case token_left_bracket:
        case token_right_bracket:
        case token_right_curly_brace:
        case token_right_parenthesis:
        case token_newline:
        case token_space:
        case token_indentation:
        case token_colon:
        case token_comma:
        case token_assign:
        case token_fat_arrow:
        case token_return:
        case token_case:
        case token_dot:
        case token_let:
        case token_equals:
        case token_not_equals:
        case token_less_than:
        case token_less_than_or_equals:
        case token_greater_than:
        case token_greater_than_or_equals:
            pop(parser);
            parser->on_error(parse_error_create(parse_error_expected_expression, head.where), parser->on_error_user);
            break;

        case token_integer:
        {
            pop(parser);
            integer value;
            if (integer_parse(&value, head.content))
            {
                expression_parser_result const result = {
                    1, expression_from_integer_literal(integer_literal_expression_create(value, head.where))};
                return result;
            }
            parser->on_error(
                parse_error_create(parse_error_integer_literal_out_of_range, head.where), parser->on_error_user);
            break;
        }

        case token_comment:
        {
            pop(parser);
            size_t end = head.content.length;
            if (head.content.begin[1] == '*')
            {
                end -= 2;
            }
            unicode_view const view = unicode_view_cut(head.content, 2, end);

            comment_expression const comment = comment_expression_create(unicode_view_copy(view), head.where);

            expression_parser_result const result = {1, expression_from_comment(comment)};
            return result;
        }
        case token_string:
        {
            pop(parser);
            expression_parser_result const result = {
                1, expression_from_string(string_expression_create(unicode_view_copy(head.content), head.where))};
            return result;
        }

        case token_raw_string:
        {
            pop(parser);
            expression_parser_result const result = {
                1, expression_from_string(string_expression_create(unicode_view_copy(head.content), head.where))};
            return result;
        }

        case token_not:
        {
            pop(parser);
            expression_parser_result const result = parse_expression(parser, indentation, false);
            if (!result.is_success)
            {
                return expression_parser_result_failure;
            }

            expression *success = allocate(sizeof(*success));
            *success = result.success;

            expression const expr = expression_from_not(not_expression_create(success));
            expression_parser_result const result1 = {1, expr};
            return result1;
        }

        case token_interface:
            pop(parser);
            return parse_interface(parser, indentation + 1, head.where);

        case token_struct:
            pop(parser);
            return parse_struct(parser, indentation + 1, head.where);

        case token_enum:
            pop(parser);
            return parse_enum(parser, indentation + 1, head.where);

        case token_impl:
            pop(parser);
            parser->on_error(parse_error_create(parse_error_expected_expression, head.where), parser->on_error_user);
            return expression_parser_result_failure;

        case token_type_of:
            pop(parser);
            return parse_type_of(parser, indentation, head.where);

        case token_import:
            pop(parser);
            return parse_import(parser, head.where);
        }
    }
}

static expression_parser_result parse_tuple(expression_parser *parser, size_t indentation,
                                            source_location const opening_brace)
{
    rich_token next = peek(parser);
    if (next.token == token_right_curly_brace)
    {
        pop(parser);
        expression_parser_result const parser_result = {
            true, expression_from_tuple(tuple_create(NULL, 0, opening_brace))};
        return parser_result;
    }

    size_t element_count = 0;
    bool more_elements = true;
    expression *tuple_elements = allocate_array(0, sizeof(*tuple_elements));
    while (next.token != token_right_curly_brace)
    {
        if (next.token == token_comma)
        {
            more_elements = true;
            pop(parser);
            parse_optional_space(parser);
        }
        expression_parser_result const parser_result = parse_expression(parser, indentation, false);
        if (parser_result.is_success && more_elements)
        {
            /*TODO: avoid O(N^2)*/
            tuple_elements = reallocate_array(tuple_elements, element_count + 1, sizeof(*tuple_elements));
            tuple_elements[element_count] = parser_result.success;
            element_count++;
            more_elements = false;
        }
        else
        {
            for (size_t i = 0; i < element_count; ++i)
            {
                expression_free(tuple_elements + i);
            }
            if (tuple_elements)
            {
                deallocate(tuple_elements);
            }
            return parser_result;
        }
        next = peek(parser);
    }
    pop(parser);
    expression_parser_result const tuple_result = {
        true, expression_from_tuple(tuple_create(tuple_elements, element_count, opening_brace))};
    return tuple_result;
}

static int parse_call(expression_parser *parser, size_t indentation, expression *result,
                      source_location const opening_parenthesis)
{
    expression *arguments = NULL;
    size_t argument_count = 0;
    int expect_another_argument = 0;
    for (;;)
    {
        {
            rich_token const maybe_close = peek(parser);
            if (is_end_of_file(&maybe_close))
            {
                pop(parser);
                parser->on_error(
                    parse_error_create(parse_error_expected_arguments, maybe_close.where), parser->on_error_user);
                if (arguments)
                {
                    deallocate(arguments);
                }
                return 0;
            }
            if (maybe_close.token == token_right_parenthesis)
            {
                pop(parser);
                if (expect_another_argument)
                {
                    parser->on_error(
                        parse_error_create(parse_error_expected_expression, maybe_close.where), parser->on_error_user);
                }
                *result = expression_from_call(call_create(expression_allocate(*result),
                                                           tuple_create(arguments, argument_count, opening_parenthesis),
                                                           maybe_close.where));
                return 1;
            }
        }
        expression_parser_result const argument = parse_expression(parser, indentation, 1);
        if (argument.is_success)
        {
            /*TODO: avoid O(N^2)*/
            arguments = reallocate_array(arguments, argument_count + 1, sizeof(*arguments));
            arguments[argument_count] = argument.success;
            argument_count++;
        }
        rich_token const maybe_comma = peek(parser);
        if (is_end_of_file(&maybe_comma))
        {
            pop(parser);
            parser->on_error(
                parse_error_create(parse_error_expected_arguments, maybe_comma.where), parser->on_error_user);
            for (size_t i = 0; i < argument_count; ++i)
            {
                expression_free(arguments + i);
            }
            if (arguments)
            {
                deallocate(arguments);
            }
            return 0;
        }
        if (maybe_comma.token == token_comma)
        {
            expect_another_argument = 1;
            pop(parser);
            rich_token const expected_space = peek(parser);
            if (expected_space.token == token_space)
            {
                pop(parser);
            }
            else
            {
                parser->on_error(
                    parse_error_create(parse_error_expected_space, expected_space.where), parser->on_error_user);
            }
        }
        else
        {
            expect_another_argument = 0;
        }
    }
}

static bool parse_instantiate_struct(expression_parser *parser, size_t indentation, expression *result,
                                     source_location const opening_brace)
{
    expression_parser_result const arguments = parse_tuple(parser, indentation, opening_brace);
    if (!arguments.is_success)
    {
        return false;
    }
    ASSUME(arguments.success.type == expression_type_tuple);
    *result = expression_from_instantiate_struct(
        instantiate_struct_expression_create(expression_allocate(*result), arguments.success.tuple));
    return true;
}

static bool parse_generic_instantiation(expression_parser *const parser, size_t const indentation,
                                        expression *const result)
{
    rich_token next = peek(parser);
    size_t argument_count = 0;
    bool more_elements = true;
    expression *arguments = allocate_array(0, sizeof(*arguments));
    while (next.token != token_right_bracket)
    {
        if (next.token == token_comma)
        {
            more_elements = true;
            pop(parser);
            next = peek(parser);
            if (next.token == token_space)
            {
                pop(parser);
            }
            else
            {
                for (size_t i = 0; i < argument_count; ++i)
                {
                    expression_free(arguments + i);
                }
                if (arguments)
                {
                    deallocate(arguments);
                }
                parser->on_error(parse_error_create(parse_error_expected_space, next.where), parser->on_error_user);
                LPG_TO_DO();
            }
        }
        expression_parser_result const parser_result = parse_expression(parser, indentation, false);
        if (parser_result.is_success && more_elements)
        {
            /*TODO: avoid O(N^2)*/
            arguments = reallocate_array(arguments, argument_count + 1, sizeof(*arguments));
            arguments[argument_count] = parser_result.success;
            argument_count++;
            more_elements = false;
        }
        else
        {
            for (size_t i = 0; i < argument_count; ++i)
            {
                expression_free(arguments + i);
            }
            if (arguments)
            {
                deallocate(arguments);
            }
            LPG_TO_DO();
        }
        next = peek(parser);
    }
    pop(parser);
    *result = expression_from_generic_instantiation(
        generic_instantiation_expression_create(expression_allocate(*result), arguments, argument_count));
    return true;
}

static expression_parser_result parse_binary_operator(expression_parser *const parser, const size_t indentation,
                                                      expression left_side, binary_operator operator)
{
    // Checking the whitespaces
    rich_token const token = peek(parser);
    if (token.token != token_space)
    {
        parser->on_error(parse_error_create(parse_error_expected_space, peek(parser).where), parser->on_error_user);
    }
    else
    {
        pop(parser);
    }

    expression_parser_result const right = parse_expression(parser, indentation, true);
    if (!right.is_success)
    {
        expression_parser_result const result = {true, left_side};
        return result;
    }

    binary_operator_expression binary_operator_expression1;
    binary_operator_expression1.left = allocate(sizeof(expression));
    *binary_operator_expression1.left = left_side;

    binary_operator_expression1.right = allocate(sizeof(*binary_operator_expression1.right));
    *binary_operator_expression1.right = right.success;

    binary_operator_expression1.comparator = operator;

    expression_parser_result const result = {true, expression_from_binary_operator(binary_operator_expression1)};
    return result;
}

static expression_parser_result parse_returnable(expression_parser *const parser, size_t const indentation,
                                                 bool const may_be_binary)
{
    expression_parser_result const callee = parse_callable(parser, indentation);
    if (!callee.is_success)
    {
        return callee;
    }
    expression_parser_result result = callee;
    for (;;)
    {
        rich_token const maybe_operator = peek(parser);
        if (maybe_operator.token == token_left_parenthesis)
        {
            pop(parser);
            if (parse_call(parser, indentation, &result.success, maybe_operator.where))
            {
                continue;
            }
        }
        if (maybe_operator.token == token_left_curly_brace)
        {
            pop(parser);
            if (parse_instantiate_struct(parser, indentation, &result.success, maybe_operator.where))
            {
                continue;
            }
        }
        if (maybe_operator.token == token_left_bracket)
        {
            pop(parser);
            if (parse_generic_instantiation(parser, indentation, &result.success))
            {
                continue;
            }
        }
        if (may_be_binary)
        {
            size_t peeking = 0;
            if ((maybe_operator.token == token_space) && (maybe_operator.content.length == 1))
            {
                peeking += 1;
            }
            rich_token const next_operator = peek_at(parser, peeking);
            if (next_operator.token == token_not_equals)
            {
                if (peeking == 0)
                {
                    parser->on_error(
                        parse_error_create(parse_error_expected_space, next_operator.where), parser->on_error_user);
                }
                pop_n(parser, peeking + 1);
                return parse_binary_operator(parser, indentation, result.success, not_equals);
            }
            if (next_operator.token == token_greater_than)
            {
                if (peeking == 0)
                {
                    parser->on_error(
                        parse_error_create(parse_error_expected_space, next_operator.where), parser->on_error_user);
                }
                pop_n(parser, peeking + 1);
                return parse_binary_operator(parser, indentation, result.success, greater_than);
            }
            if (next_operator.token == token_greater_than_or_equals)
            {
                if (peeking == 0)
                {
                    parser->on_error(
                        parse_error_create(parse_error_expected_space, next_operator.where), parser->on_error_user);
                }
                pop_n(parser, peeking + 1);
                return parse_binary_operator(parser, indentation, result.success, greater_than_or_equals);
            }
            if (next_operator.token == token_less_than)
            {
                if (peeking == 0)
                {
                    parser->on_error(
                        parse_error_create(parse_error_expected_space, next_operator.where), parser->on_error_user);
                }
                pop_n(parser, peeking + 1);
                return parse_binary_operator(parser, indentation, result.success, less_than);
            }
            if (next_operator.token == token_less_than_or_equals)
            {
                if (peeking == 0)
                {
                    parser->on_error(
                        parse_error_create(parse_error_expected_space, next_operator.where), parser->on_error_user);
                }
                pop_n(parser, peeking + 1);
                return parse_binary_operator(parser, indentation, result.success, less_than_or_equals);
            }
            if (next_operator.token == token_equals)
            {
                if (peeking == 0)
                {
                    parser->on_error(
                        parse_error_create(parse_error_expected_space, next_operator.where), parser->on_error_user);
                }
                pop_n(parser, peeking + 1);
                return parse_binary_operator(parser, indentation, result.success, equals);
            }
        }
        if (maybe_operator.token == token_dot)
        {
            pop(parser);
            rich_token const element_name = peek(parser);
            pop(parser);
            if ((element_name.token == token_identifier) || (element_name.token == token_integer))
            {
                expression const access = expression_from_access_structure(access_structure_create(
                    expression_allocate(result.success),
                    identifier_expression_create(unicode_view_copy(element_name.content), element_name.where)));
                result.success = access;
                continue;
            }
            parser->on_error(
                parse_error_create(parse_error_expected_element_name, element_name.where), parser->on_error_user);
            ASSUME(result.is_success);
            expression_free(&result.success);
            return expression_parser_result_failure;
        }
        return result;
    }
}

static expression_parser_result parse_impl(expression_parser *const parser, size_t const indentation)
{
    {
        rich_token const space = peek(parser);
        pop(parser);
        if (space.token != token_space)
        {
            parser->on_error(parse_error_create(parse_error_expected_space, space.where), parser->on_error_user);
        }
    }

    expression_parser_result const implemented_interface = parse_expression(parser, indentation, false);
    if (!implemented_interface.is_success)
    {
        return expression_parser_result_failure;
    }

    {
        rich_token const space = peek(parser);
        pop(parser);
        if (space.token != token_space || space.content.length != 1)
        {
            parser->on_error(parse_error_create(parse_error_expected_space, space.where), parser->on_error_user);
            expression_free(&implemented_interface.success);
            return expression_parser_result_failure;
        }
    }

    {
        rich_token const for_ = peek(parser);
        pop(parser);
        if ((for_.token != token_identifier) || !unicode_view_equals_c_str(for_.content, "for"))
        {
            parser->on_error(parse_error_create(parse_error_expected_for, for_.where), parser->on_error_user);
            expression_free(&implemented_interface.success);
            return expression_parser_result_failure;
        }
    }

    {
        rich_token const space = peek(parser);
        pop(parser);
        if (space.token != token_space)
        {
            parser->on_error(parse_error_create(parse_error_expected_space, space.where), parser->on_error_user);
            expression_free(&implemented_interface.success);
            return expression_parser_result_failure;
        }
    }

    expression_parser_result const self = parse_expression(parser, indentation, false);
    if (!self.is_success)
    {
        expression_free(&implemented_interface.success);
        return expression_parser_result_failure;
    }

    {
        rich_token const newline = peek_at(parser, 0);
        pop(parser);
        if (newline.token != token_newline)
        {
            parser->on_error(parse_error_create(parse_error_expected_newline, newline.where), parser->on_error_user);
            expression_free(&implemented_interface.success);
            expression_free(&self.success);
            return expression_parser_result_failure;
        }
    }

    impl_expression_method *methods = NULL;
    size_t method_count = 0;
    bool is_success = true;
    for (;;)
    {
        size_t const method_indentation = (indentation + 1);

        {
            rich_token const indent = peek_at(parser, 0);
            if (!is_same_indentation_level(indent, method_indentation))
            {
                break;
            }
        }
        pop_n(parser, 1);
        rich_token const name = peek(parser);
        if (name.token != token_identifier)
        {
            parser->on_error(parse_error_create(parse_error_expected_identifier, name.where), parser->on_error_user);
            is_success = false;
            break;
        }
        pop(parser);

        {
            rich_token const left_parenthesis = peek(parser);
            if (left_parenthesis.token != token_left_parenthesis)
            {
                parser->on_error(parse_error_create(parse_error_expected_parameter_list, left_parenthesis.where),
                                 parser->on_error_user);
                is_success = false;
                break;
            }
            pop(parser);
        }

        function_header_parse_result const header_parsed = parse_function_header(parser, method_indentation);
        if (!header_parsed.is_success)
        {
            is_success = false;
            break;
        }

        {
            rich_token const new_line = peek(parser);
            if (new_line.token != token_newline)
            {
                parser->on_error(
                    parse_error_create(parse_error_expected_newline, new_line.where), parser->on_error_user);
                is_success = false;
                break;
            }
            pop(parser);
        }

        sequence const body = parse_sequence(parser, method_indentation + 1);

        methods = reallocate_array(methods, (method_count + 1), sizeof(*methods));
        methods[method_count] = impl_expression_method_create(
            identifier_expression_create(unicode_view_copy(name.content), name.where), header_parsed.header, body);
        ++method_count;
    }
    impl_expression const impl = impl_expression_create(
        expression_allocate(implemented_interface.success), expression_allocate(self.success), methods, method_count);
    if (is_success)
    {
        expression_parser_result const result = {true, expression_from_impl(impl)};
        return result;
    }
    impl_expression_free(impl);
    return expression_parser_result_failure;
}

expression_parser_result parse_let_expression(expression_parser *const parser, size_t const indentation)
{
    if (!parse_space(parser))
    {
        return expression_parser_result_failure;
    }
    rich_token const name = peek(parser);
    pop(parser);
    if (name.token != token_identifier)
    {
        parser->on_error(parse_error_create(parse_error_expected_identifier, name.where), parser->on_error_user);
        return expression_parser_result_failure;
    }

    if (peek(parser).token == token_right_parenthesis)
    {
        expression_parser_result const result = {true, expression_from_placeholder(placeholder_expression_create(
                                                           name.where, unicode_view_copy(name.content)))};
        return result;
    }
    parse_optional_space(parser);
    expression_parser_result declared_variable_type = {false, expression_from_break(source_location_create(0, 0))};
    rich_token colon_or_assign = peek(parser);
    if (colon_or_assign.token == token_colon)
    {
        pop(parser);
        parse_optional_space(parser);
        declared_variable_type = parse_returnable(parser, indentation, false);
        if (!declared_variable_type.is_success)
        {
            return expression_parser_result_failure;
        }
        parse_optional_space(parser);
        colon_or_assign = peek(parser);
    }
    if (colon_or_assign.token != token_assign)
    {
        if (declared_variable_type.is_success)
        {
            expression_free(&declared_variable_type.success);
        }
        parser->on_error(parse_error_create(parse_error_expected_declaration_or_assignment, colon_or_assign.where),
                         parser->on_error_user);
        return expression_parser_result_failure;
    }
    pop(parser);
    parse_optional_space(parser);
    expression_parser_result const initial_value = parse_returnable(parser, indentation, true);
    if (!initial_value.is_success)
    {
        if (declared_variable_type.is_success)
        {
            expression_free(&declared_variable_type.success);
        }
        return expression_parser_result_failure;
    }
    expression_parser_result const result = {
        true, expression_from_declare(declare_create(
                  identifier_expression_create(unicode_view_copy(name.content), name.where),
                  (declared_variable_type.is_success ? expression_allocate(declared_variable_type.success) : NULL),
                  expression_allocate(initial_value.success)))};
    return result;
}

expression_parser_result parse_expression(expression_parser *const parser, size_t const indentation,
                                          bool const may_be_statement)
{
    if (may_be_statement)
    {
        rich_token const head = peek(parser);
        if (head.token == token_return)
        {
            pop(parser);
            rich_token const space = peek(parser);
            pop(parser);
            if ((space.token == token_space) && (space.content.length == 1))
            {
                expression_parser_result result = parse_returnable(parser, indentation, true);
                if (result.is_success)
                {
                    result.success = expression_from_return(expression_allocate(result.success));
                }
                return result;
            }
            parser->on_error(parse_error_create(parse_error_expected_space, space.where), parser->on_error_user);
            return expression_parser_result_failure;
        }
        if (head.token == token_let)
        {
            pop(parser);
            return parse_let_expression(parser, indentation);
        }
        if (head.token == token_impl)
        {
            pop(parser);
            return parse_impl(parser, indentation);
        }
    }
    return parse_returnable(parser, indentation, true);
}

sequence parse_program(expression_parser *parser)
{
    return parse_sequence(parser, 0);
}
