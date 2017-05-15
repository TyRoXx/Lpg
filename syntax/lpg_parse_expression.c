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
        expression_parser_result const result = {0, expression_from_break()};
        return result;
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
                                            size_t const indentation)
{
    {
        rich_token const space = peek(parser);
        pop(parser);
        if (space.token != token_space)
        {
            parser->on_error(
                parse_error_create(parse_error_expected_space, space.where),
                parser->user);
            expression_parser_result const result = {
                0, expression_from_break()};
            return result;
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
            expression_parser_result const result = {
                0, expression_from_break()};
            expression_free(&input.success);
            return result;
        }
    }
    match_case *cases = NULL;
    size_t case_count = 0;
    if (parse_match_cases(parser, (indentation + 1), &cases, &case_count))
    {
        expression_parser_result const result = {
            1, expression_from_match(match_create(
                   expression_allocate(input.success), cases, case_count))};
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
    expression_parser_result const result = {0, expression_from_break()};
    return result;
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
            expression_parser_result const result = {
                0, expression_from_break()};
            return result;
        }
        switch (head.token)
        {
        case token_identifier:
        {
            pop(parser);
            expression_parser_result result = {
                1, expression_from_identifier(identifier_expression_create(
                       unicode_view_copy(head.content), head.where))};
            return result;
        }

        case token_break:
        {
            pop(parser);
            expression_parser_result result = {1, expression_from_break()};
            return result;
        }

        case token_loop:
            pop(parser);
            return parse_loop(parser, indentation);

        case token_match:
            pop(parser);
            return parse_match(parser, indentation);

        case token_newline:
        case token_space:
        case token_indentation:
        case token_left_parenthesis:
        case token_right_parenthesis:
        case token_colon:
        case token_comma:
        case token_assign:
        case token_fat_arrow:
        case token_return:
        case token_case:
        case token_dot:
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
                expression_parser_result result = {
                    1, expression_from_integer_literal(value)};
                return result;
            }
            parser->on_error(
                parse_error_create(
                    parse_error_integer_literal_out_of_range, head.where),
                parser->user);
            break;
        }

        case token_string:
        {
            pop(parser);
            expression_parser_result result = {
                1, expression_from_unicode_string(
                       unicode_view_copy(head.content))};
            return result;
        }
        }
    }
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
                                tuple_create(arguments, argument_count)));
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
        expression_parser_result assign_result = {
            1, expression_from_assign(
                   assign_create(expression_allocate(left_side),
                                 expression_allocate(value.success)))};
        return assign_result;
    }
    expression_parser_result result = {1, left_side};
    return result;
}

static expression_parser_result parse_declaration(expression_parser *parser,
                                                  size_t indentation,
                                                  expression const name)
{
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
    }
    expression_parser_result const type =
        parse_expression(parser, indentation, 0);
    if (!type.is_success)
    {
        expression_parser_result result = {1, name};
        return result;
    }
    {
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
    {
        rich_token const expected_assign = peek(parser);
        if (expected_assign.token == token_assign)
        {
            pop(parser);
        }
        else
        {
            parser->on_error(parse_error_create(parse_error_expected_assignment,
                                                expected_assign.where),
                             parser->user);
        }
    }
    {
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
    expression_parser_result const value =
        parse_expression(parser, indentation, 0);
    if (!value.is_success)
    {
        expression_free(&name);
        expression_free(&type.success);
        return value;
    }
    expression_parser_result assign_result = {
        1, expression_from_declare(declare_create(
               expression_allocate(name), expression_allocate(type.success),
               expression_allocate(value.success)))};
    return assign_result;
}

static expression_parser_result parse_returnable(expression_parser *parser,
                                                 size_t indentation,
                                                 int may_be_statement)
{
    expression_parser_result callee = parse_callable(parser, indentation);
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
            if (maybe_operator.token == token_colon)
            {
                pop(parser);
                parser->on_error(parse_error_create(parse_error_expected_space,
                                                    maybe_operator.where),
                                 parser->user);
                return parse_declaration(parser, indentation, result.success);
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
                if (next_operator.token == token_colon)
                {
                    return parse_declaration(
                        parser, indentation, result.success);
                }
                parser->on_error(
                    parse_error_create(
                        parse_error_expected_declaration_or_assignment,
                        next_operator.where),
                    parser->user);
                return result;
            }
            if (maybe_operator.token == token_dot)
            {
                pop(parser);
                rich_token const element_name = peek(parser);
                pop(parser);
                if (element_name.token == token_identifier)
                {
                    expression access = expression_from_access_structure(
                        access_structure_create(
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
                return result;
            }
        }
        return result;
    }
}

expression_parser_result parse_expression(expression_parser *parser,
                                          size_t indentation,
                                          bool may_be_statement)
{
    if (may_be_statement)
    {
        rich_token const return_ = peek(parser);
        if (return_.token == token_return)
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
            expression_parser_result result = {0, expression_from_break()};
            return result;
        }
    }
    return parse_returnable(parser, indentation, may_be_statement);
}

sequence parse_program(expression_parser *parser)
{
    return parse_sequence(parser, 0);
}
