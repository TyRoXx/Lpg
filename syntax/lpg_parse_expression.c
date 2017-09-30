#include "lpg_parse_expression.h"
#include "lpg_assert.h"
#include "lpg_allocate.h"
#include "lpg_for.h"

rich_token rich_token_create(tokenize_status status, token_type token,
                             unicode_view content, source_location where)
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
    parse_error result = {type, where};
    return result;
}

bool parse_error_equals(parse_error left, parse_error right)
{
    return (left.type == right.type) &&
           source_location_equals(left.where, right.where);
}

expression_parser expression_parser_create(rich_token_producer find_next_token,
                                           parse_error_handler on_error,
                                           callback_user user)
{
    expression_parser result = {find_next_token,
                                on_error,
                                user,
                                0,
                                {tokenize_success, 0, {NULL, 0}, {0, 0}}};
    return result;
}

static rich_token peek(expression_parser *parser)
{
    while (!parser->has_cached_token)
    {
        parser->cached_token = parser->find_next_token(parser->user);
        switch (parser->cached_token.status)
        {
        case tokenize_success:
            parser->has_cached_token = 1;
            break;

        case tokenize_invalid:
            parser->on_error(parse_error_create(parse_error_invalid_token,
                                                parser->cached_token.where),
                             parser->user);
            break;
        }
    }
    ASSUME(parser->cached_token.status == tokenize_success);
    return parser->cached_token;
}

static void pop(expression_parser *parser)
{
    ASSUME(parser->has_cached_token);
    parser->has_cached_token = 0;
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
        if ((indentation == 0) ||
            ((indentation_token.token == token_indentation) &&
             (indentation_token.content.length ==
              (indentation * spaces_for_indentation))))
        {
            if (indentation > 0)
            {
                pop(parser);
            }
            expression_parser_result const element =
                parse_expression(parser, indentation, 1);
            if (element.is_success)
            {
                /*TODO: avoid O(N^2)*/
                elements = reallocate_array(
                    elements, (element_count + 1), sizeof(*elements));
                elements[element_count] = element.success;
                ++element_count;
            }
            rich_token const maybe_newline = peek(parser);
            if (maybe_newline.token == token_newline)
            {
                pop(parser);
            }
        }
        else
        {
            break;
        }
    }
    return sequence_create(elements, element_count);
}

static expression_parser_result const expression_parser_result_failure = {
    0, {expression_type_lambda, {{NULL, 0, NULL}}}};

static expression_parser_result parse_tuple(expression_parser *parser,
                                            size_t indentation);

static expression_parser_result parse_loop(expression_parser *parser,
                                           size_t indentation)
{
    rich_token const maybe_newline = peek(parser);
    pop(parser);
    if (maybe_newline.token != token_newline)
    {
        parser->on_error(parse_error_create(
                             parse_error_expected_newline, maybe_newline.where),
                         parser->user);
        return expression_parser_result_failure;
    }
    expression_parser_result const result = {
        1, expression_from_loop(parse_sequence(parser, (indentation + 1)))};
    return result;
}

static int parse_match_cases(expression_parser *parser,
                             size_t const indentation, match_case **cases,
                             size_t *case_count)
{
    for (;;)
    {
        rich_token const indentation_token = peek(parser);
        if ((indentation_token.token != token_indentation) ||
            (indentation_token.content.length !=
             (indentation * spaces_for_indentation)))
        {
            return 1;
        }
        pop(parser);
        rich_token const expected_case = peek(parser);
        if (expected_case.token != token_case)
        {
            parser->on_error(parse_error_create(parse_error_expected_case,
                                                expected_case.where),
                             parser->user);
            return 0;
        }
        pop(parser);
        {
            rich_token const space = peek(parser);
            pop(parser);
            if (space.token != token_space)
            {
                parser->on_error(
                    parse_error_create(parse_error_expected_space, space.where),
                    parser->user);
                return 0;
            }
        }
        expression_parser_result const key =
            parse_expression(parser, indentation, 0);
        if (!key.is_success)
        {
            return 0;
        }
        {
            rich_token const colon = peek(parser);
            pop(parser);
            if (colon.token != token_colon)
            {
                parser->on_error(
                    parse_error_create(parse_error_expected_colon, colon.where),
                    parser->user);
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
                    parse_error_create(
                        parse_error_expected_expression, peek(parser).where),
                    parser->user);
                return 0;
            }
            *cases =
                reallocate_array(*cases, (*case_count + 1), sizeof(**cases));
            (*cases)[*case_count] = match_case_create(
                expression_allocate(key.success),
                expression_allocate(expression_from_sequence(value)));
        }
        else
        {
            if (space.token == token_space)
            {
                pop(parser);
            }
            else
            {
                parser->on_error(
                    parse_error_create(parse_error_expected_space, space.where),
                    parser->user);
            }

            expression_parser_result const value =
                parse_expression(parser, indentation, 0);
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
                        parse_error_create(
                            parse_error_expected_newline, newline.where),
                        parser->user);
                    return 0;
                }
            }
            *cases =
                reallocate_array(*cases, (*case_count + 1), sizeof(**cases));
            (*cases)[*case_count] =
                match_case_create(expression_allocate(key.success),
                                  expression_allocate(value.success));
        }
        ++(*case_count);
    }
}

static expression_parser_result parse_match(expression_parser *parser,
                                            size_t const indentation,
                                            source_location const begin)
{
    {
        rich_token const space = peek(parser);
        pop(parser);
        if (space.token != token_space)
        {
            parser->on_error(
                parse_error_create(parse_error_expected_space, space.where),
                parser->user);
            return expression_parser_result_failure;
        }
    }
    expression_parser_result const input =
        parse_expression(parser, indentation, 0);
    if (!input.is_success)
    {
        return input;
    }
    {
        rich_token const newline = peek(parser);
        pop(parser);
        if (newline.token != token_newline)
        {
            parser->on_error(
                parse_error_create(parse_error_expected_newline, newline.where),
                parser->user);
            expression_free(&input.success);
            return expression_parser_result_failure;
        }
    }
    match_case *cases = NULL;
    size_t case_count = 0;
    if (parse_match_cases(parser, (indentation + 1), &cases, &case_count))
    {
        expression_parser_result const result = {
            1,
            expression_from_match(match_create(
                begin, expression_allocate(input.success), cases, case_count))};
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

static void clean_up_parameters(parameter *const parameters,
                                size_t parameter_count)
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

static expression_parser_result parse_lambda(expression_parser *const parser,
                                             size_t const indentation)
{
    parameter *parameters = NULL;
    size_t parameter_count = 0;
    for (;;)
    {
        rich_token const head = peek(parser);
        if (head.token == token_right_parenthesis)
        {
            pop(parser);
            rich_token const whitespace = peek(parser);
            if (whitespace.token == token_space)
            {
                pop(parser);
                expression_parser_result const body =
                    parse_expression(parser, indentation, false);
                if (!body.is_success)
                {
                    return expression_parser_result_failure;
                }
                expression_parser_result const result = {
                    1, expression_from_lambda(
                           lambda_create(parameters, parameter_count,
                                         expression_allocate(body.success)))};
                return result;
            }
            if (whitespace.token == token_newline)
            {
                pop(parser);
                sequence const body = parse_sequence(parser, (indentation + 1));
                expression_parser_result const result = {
                    1,
                    expression_from_lambda(lambda_create(
                        parameters, parameter_count,
                        expression_allocate(expression_from_sequence(body))))};
                return result;
            }
            pop(parser);
            parser->on_error(
                parse_error_create(
                    parse_error_expected_lambda_body, whitespace.where),
                parser->user);
            clean_up_parameters(parameters, parameter_count);
            return expression_parser_result_failure;
        }
        if (parameter_count >= 1)
        {
            if (head.token == token_comma)
            {
                pop(parser);
                rich_token const space = peek(parser);
                if (space.token == token_space)
                {
                    pop(parser);
                }
                else
                {
                    parser->on_error(
                        parse_error_create(
                            parse_error_expected_space, space.where),
                        parser->user);
                    clean_up_parameters(parameters, parameter_count);
                    return expression_parser_result_failure;
                }
            }
            else
            {
                pop(parser);
                parser->on_error(
                    parse_error_create(parse_error_expected_comma, head.where),
                    parser->user);
                clean_up_parameters(parameters, parameter_count);
                return expression_parser_result_failure;
            }
        }

        rich_token const name = peek(parser);
        pop(parser);
        if (name.token != token_identifier)
        {
            parser->on_error(
                parse_error_create(parse_error_expected_identifier, name.where),
                parser->user);
            clean_up_parameters(parameters, parameter_count);
            return expression_parser_result_failure;
        }

        {
            rich_token const colon = peek(parser);
            pop(parser);
            if (colon.token != token_colon)
            {
                parser->on_error(
                    parse_error_create(parse_error_expected_colon, colon.where),
                    parser->user);
                clean_up_parameters(parameters, parameter_count);
                return expression_parser_result_failure;
            }
        }

        {
            rich_token const space = peek(parser);
            pop(parser);
            if (space.token != token_space)
            {
                parser->on_error(
                    parse_error_create(parse_error_expected_space, space.where),
                    parser->user);
                clean_up_parameters(parameters, parameter_count);
                return expression_parser_result_failure;
            }
        }

        expression_parser_result const parsed_type =
            parse_expression(parser, indentation, false);
        if (!parsed_type.is_success)
        {
            clean_up_parameters(parameters, parameter_count);
            return expression_parser_result_failure;
        }

        parameters = reallocate_array(
            parameters, (parameter_count + 1), sizeof(*parameters));
        parameters[parameter_count] =
            parameter_create(identifier_expression_create(
                                 unicode_view_copy(name.content), name.where),
                             expression_allocate(parsed_type.success));
        ++parameter_count;
    }
}

static expression_parser_result parse_callable(expression_parser *parser,
                                               size_t indentation)
{
    for (;;)
    {
        rich_token const head = peek(parser);
        if (is_end_of_file(&head))
        {
            pop(parser);
            parser->on_error(
                parse_error_create(parse_error_expected_expression, head.where),
                parser->user);
            return expression_parser_result_failure;
        }
        switch (head.token)
        {
        case token_identifier:
        {
            pop(parser);
            expression_parser_result const result = {
                1, expression_from_identifier(identifier_expression_create(
                       unicode_view_copy(head.content), head.where))};
            return result;
        }

        case token_break:
        {
            pop(parser);
            expression_parser_result const result = {
                1, expression_from_break(head.where)};
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
            return parse_lambda(parser, indentation);

        case token_left_curly_brace:
        {
            pop(parser);
            return parse_tuple(parser, indentation);
        }

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
            parser->on_error(
                parse_error_create(parse_error_expected_expression, head.where),
                parser->user);
            break;

        case token_integer:
        {
            pop(parser);
            integer value;
            if (integer_parse(&value, head.content))
            {
                expression_parser_result const result = {
                    1,
                    expression_from_integer_literal(
                        integer_literal_expression_create(value, head.where))};
                return result;
            }
            parser->on_error(
                parse_error_create(
                    parse_error_integer_literal_out_of_range, head.where),
                parser->user);
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

            comment_expression const comment =
                comment_expression_create(unicode_view_copy(view), head.where);

            expression_parser_result const result = {
                1, expression_from_comment(comment)};
            return result;
        }
        case token_string:
        {
            pop(parser);
            expression_parser_result const result = {
                1, expression_from_string(string_expression_create(
                       unicode_view_copy(head.content), head.where))};
            return result;
        }
        case token_not:
        {
            pop(parser);
            expression_parser_result result =
                parse_expression(parser, indentation, false);
            if (!result.is_success)
            {
                return expression_parser_result_failure;
            }

            expression *success = allocate(sizeof(*success));
            *success = result.success;

            expression expr =
                expression_from_not(not_expression_create(success));
            expression_parser_result result1 = {1, expr};
            return result1;
        };
        }
    }
}

static expression_parser_result parse_tuple(expression_parser *parser,
                                            size_t indentation)
{
    rich_token next = peek(parser);
    if (next.token == token_right_curly_brace)
    {
        pop(parser);
        expression_parser_result const parser_result = {
            true, expression_from_tuple(tuple_create(NULL, 0))};
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
            next = peek(parser);
            if (next.token == token_space)
            {
                pop(parser);
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
                parser->on_error(
                    parse_error_create(parse_error_expected_space, next.where),
                    parser->user);
                return expression_parser_result_failure;
            }
        }
        expression_parser_result const parser_result =
            parse_expression(parser, indentation, false);
        if (parser_result.is_success && more_elements)
        {
            /*TODO: avoid O(N^2)*/
            tuple_elements = reallocate_array(
                tuple_elements, element_count + 1, sizeof(*tuple_elements));
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
        true,
        expression_from_tuple(tuple_create(tuple_elements, element_count))};
    return tuple_result;
}

static int parse_call(expression_parser *parser, size_t indentation,
                      expression *result)
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
                    parse_error_create(
                        parse_error_expected_arguments, maybe_close.where),
                    parser->user);
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
                        parse_error_create(
                            parse_error_expected_expression, maybe_close.where),
                        parser->user);
                }
                *result = expression_from_call(
                    call_create(expression_allocate(*result),
                                tuple_create(arguments, argument_count),
                                maybe_close.where));
                return 1;
            }
        }
        expression_parser_result const argument =
            parse_expression(parser, indentation, 1);
        if (argument.is_success)
        {
            /*TODO: avoid O(N^2)*/
            arguments = reallocate_array(
                arguments, argument_count + 1, sizeof(*arguments));
            arguments[argument_count] = argument.success;
            argument_count++;
        }
        rich_token const maybe_comma = peek(parser);
        if (is_end_of_file(&maybe_comma))
        {
            pop(parser);
            parser->on_error(parse_error_create(parse_error_expected_arguments,
                                                maybe_comma.where),
                             parser->user);
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
                parser->on_error(parse_error_create(parse_error_expected_space,
                                                    expected_space.where),
                                 parser->user);
            }
        }
        else
        {
            expect_another_argument = 0;
        }
    }
}

static expression_parser_result parse_assignment(expression_parser *parser,
                                                 size_t indentation,
                                                 expression const left_side)
{
    rich_token const space = peek(parser);
    if (space.token == token_space)
    {
        pop(parser);
    }
    else
    {
        parser->on_error(
            parse_error_create(parse_error_expected_space, space.where),
            parser->user);
    }
    expression_parser_result const value =
        parse_expression(parser, indentation, 0);
    if (value.is_success)
    {
        expression_parser_result const assign_result = {
            1, expression_from_assign(
                   assign_create(expression_allocate(left_side),
                                 expression_allocate(value.success)))};
        return assign_result;
    }
    expression_parser_result const result = {1, left_side};
    return result;
}

static expression_parser_result
parse_binary_operator(expression_parser *const parser, const size_t indentation,
                      expression expression, binary_operator operator)
{
    // Checking the whitespaces
    rich_token token = peek(parser);
    if (token.token != token_space)
    {
        parser->on_error(
            parse_error_create(parse_error_expected_space, peek(parser).where),
            parser->user);
    }
    else
    {
        pop(parser);
    }

    binary_operator_expression binary_operator_expression1;
    binary_operator_expression1.left = allocate(sizeof(expression));
    *binary_operator_expression1.left = expression;

    expression_parser_result right =
        parse_expression(parser, indentation, true);
    if (!right.is_success)
    {
        return expression_parser_result_failure;
    }

    binary_operator_expression1.right =
        allocate(sizeof(*binary_operator_expression1.right));
    *binary_operator_expression1.right = right.success;

    binary_operator_expression1.comparator = operator;

    expression_parser_result result = {
        1, expression_from_binary_operator(binary_operator_expression1)};
    return result;
}

static expression_parser_result
parse_returnable(expression_parser *const parser, size_t const indentation,
                 bool const may_be_statement)
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
            if (parse_call(parser, indentation, &result.success))
            {
                continue;
            }
        }
        if (may_be_statement)
        {
            if (maybe_operator.token == token_assign)
            {
                pop(parser);
                parser->on_error(parse_error_create(parse_error_expected_space,
                                                    maybe_operator.where),
                                 parser->user);
                return parse_assignment(parser, indentation, result.success);
            }
            if ((maybe_operator.token == token_space) &&
                (maybe_operator.content.length == 1))
            {
                pop(parser);
                rich_token const next_operator = peek(parser);
                pop(parser);
                if (next_operator.token == token_assign)
                {
                    return parse_assignment(
                        parser, indentation, result.success);
                }
                else if (next_operator.token == token_not_equals)
                {
                    return parse_binary_operator(
                        parser, indentation, result.success, not_equals);
                }
                else if (next_operator.token == token_greater_than)
                {
                    return parse_binary_operator(
                        parser, indentation, result.success, greater_than);
                }
                else if (next_operator.token == token_greater_than_or_equals)
                {
                    return parse_binary_operator(parser, indentation,
                                                 result.success,
                                                 greater_than_or_equals);
                }
                else if (next_operator.token == token_less_than)
                {
                    return parse_binary_operator(
                        parser, indentation, result.success, less_than);
                }
                else if (next_operator.token == token_less_than_or_equals)
                {
                    return parse_binary_operator(parser, indentation,
                                                 result.success,
                                                 less_than_or_equals);
                }
                else if (next_operator.token == token_equals)
                {
                    return parse_binary_operator(
                        parser, indentation, result.success, equals);
                }
                parser->on_error(
                    parse_error_create(
                        parse_error_expected_declaration_or_assignment,
                        next_operator.where),
                    parser->user);
                return result;
            }
        }
        if (maybe_operator.token == token_dot)
        {
            pop(parser);
            rich_token const element_name = peek(parser);
            pop(parser);
            if ((element_name.token == token_identifier) ||
                (element_name.token == token_integer))
            {
                expression const access =
                    expression_from_access_structure(access_structure_create(
                        expression_allocate(result.success),
                        identifier_expression_create(
                            unicode_view_copy(element_name.content),
                            element_name.where)));
                result.success = access;
                continue;
            }
            parser->on_error(
                parse_error_create(
                    parse_error_expected_element_name, element_name.where),
                parser->user);
            ASSUME(result.is_success);
            expression_free(&result.success);
            return expression_parser_result_failure;
        }
        return result;
    }
}

expression_parser_result parse_expression(expression_parser *const parser,
                                          size_t const indentation,
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
                expression_parser_result result =
                    parse_returnable(parser, indentation, 0);
                if (result.is_success)
                {
                    result.success = expression_from_return(
                        expression_allocate(result.success));
                }
                return result;
            }
            parser->on_error(
                parse_error_create(parse_error_expected_space, space.where),
                parser->user);
            return expression_parser_result_failure;
        }
        if (head.token == token_let)
        {
            pop(parser);
            rich_token const first_space = peek(parser);
            pop(parser);
            if ((first_space.token != token_space) ||
                (first_space.content.length != 1))
            {
                parser->on_error(parse_error_create(parse_error_expected_space,
                                                    first_space.where),
                                 parser->user);
                return expression_parser_result_failure;
            }
            rich_token const name = peek(parser);
            pop(parser);
            if (name.token != token_identifier)
            {
                parser->on_error(
                    parse_error_create(
                        parse_error_expected_identifier, name.where),
                    parser->user);
                return expression_parser_result_failure;
            }
            {
                rich_token const second_space = peek(parser);
                if ((second_space.token == token_space) &&
                    (second_space.content.length == 1))
                {
                    pop(parser);
                }
                else
                {
                    parser->on_error(
                        parse_error_create(
                            parse_error_expected_space, second_space.where),
                        parser->user);
                }
            }
            expression_parser_result declared_variable_type = {
                false, expression_from_break(source_location_create(0, 0))};
            rich_token colon_or_assign = peek(parser);
            if (colon_or_assign.token == token_colon)
            {
                pop(parser);
                rich_token const third_space = peek(parser);
                if ((third_space.token == token_space) &&
                    (third_space.content.length == 1))
                {
                    pop(parser);
                }
                else
                {
                    parser->on_error(
                        parse_error_create(
                            parse_error_expected_space, third_space.where),
                        parser->user);
                }
                declared_variable_type =
                    parse_returnable(parser, indentation, false);
                if (!declared_variable_type.is_success)
                {
                    return expression_parser_result_failure;
                }
                rich_token const fourth_space = peek(parser);
                if ((fourth_space.token == token_space) &&
                    (fourth_space.content.length == 1))
                {
                    pop(parser);
                }
                else
                {
                    parser->on_error(
                        parse_error_create(
                            parse_error_expected_space, fourth_space.where),
                        parser->user);
                }
                colon_or_assign = peek(parser);
            }
            if (colon_or_assign.token != token_assign)
            {
                if (declared_variable_type.is_success)
                {
                    expression_free(&declared_variable_type.success);
                }
                parser->on_error(
                    parse_error_create(
                        parse_error_expected_declaration_or_assignment,
                        colon_or_assign.where),
                    parser->user);
                return expression_parser_result_failure;
            }
            pop(parser);
            rich_token const another_space = peek(parser);
            if ((another_space.token == token_space) &&
                (another_space.content.length == 1))
            {
                pop(parser);
            }
            else
            {
                parser->on_error(parse_error_create(parse_error_expected_space,
                                                    another_space.where),
                                 parser->user);
            }
            expression_parser_result const initial_value =
                parse_returnable(parser, indentation, false);
            if (!initial_value.is_success)
            {
                if (declared_variable_type.is_success)
                {
                    expression_free(&declared_variable_type.success);
                }
                return expression_parser_result_failure;
            }
            expression_parser_result const result = {
                true,
                expression_from_declare(declare_create(
                    identifier_expression_create(
                        unicode_view_copy(name.content), name.where),
                    (declared_variable_type.is_success
                         ? expression_allocate(declared_variable_type.success)
                         : NULL),
                    expression_allocate(initial_value.success)))};
            return result;
        }
    }
    return parse_returnable(parser, indentation, may_be_statement);
}

sequence parse_program(expression_parser *parser)
{
    return parse_sequence(parser, 0);
}
