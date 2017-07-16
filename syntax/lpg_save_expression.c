#include "lpg_save_expression.h"
#include "lpg_unicode_view.h"
#include "lpg_assert.h"
#include "lpg_for.h"
#include "lpg_identifier.h"

static success_indicator indent(const stream_writer to,
                                whitespace_state const whitespace)
{
    LPG_FOR(size_t, j, whitespace.indentation_depth)
    {
        LPG_TRY(stream_writer_write_string(to, "    "));
    }
    return success;
}

static whitespace_state go_deeper(whitespace_state const whitespace,
                                  size_t additional_indentation)
{
    whitespace_state result = {
        whitespace.indentation_depth + additional_indentation, 0};
    return result;
}

static whitespace_state add_space_or_newline(whitespace_state const whitespace)
{
    whitespace_state result = {whitespace.indentation_depth, 1};
    return result;
}

static success_indicator space_here(stream_writer const to,
                                    whitespace_state *whitespace)
{
    if (whitespace->pending_space)
    {
        whitespace->pending_space = 0;
        return stream_writer_write_string(to, " ");
    }
    return success;
}

success_indicator save_expression(stream_writer const to,
                                  expression const *value,
                                  whitespace_state whitespace)
{
    switch (value->type)
    {
    case expression_type_lambda:
        LPG_TRY(space_here(to, &whitespace));
        LPG_TRY(stream_writer_write_string(to, "("));
        LPG_FOR(size_t, i, value->lambda.parameter_count)
        {
            if (i > 0)
            {
                LPG_TRY(stream_writer_write_string(to, ", "));
            }
            parameter const *param = value->lambda.parameters + i;
            LPG_TRY(stream_writer_write_bytes(
                to, param->name.value.data, param->name.value.length));
            LPG_TRY(stream_writer_write_string(to, ": "));
            LPG_TRY(save_expression(to, param->type, whitespace));
        }
        LPG_TRY(stream_writer_write_string(to, ") => "));
        LPG_TRY(save_expression(to, value->lambda.result, whitespace));
        return success;

    case expression_type_call:
    {
        LPG_TRY(save_expression(to, value->call.callee, whitespace));
        expression const arguments =
            expression_from_tuple(value->call.arguments);
        return save_expression(to, &arguments, whitespace);
    }

    case expression_type_integer_literal:
    {
        LPG_TRY(space_here(to, &whitespace));
        char buffer[39];
        char *const formatted =
            integer_format(value->integer_literal.value, lower_case_digits, 10,
                           buffer, sizeof(buffer));
        return stream_writer_write_bytes(
            to, formatted, (size_t)(buffer + sizeof(buffer) - formatted));
    }

    case expression_type_access_structure:
    {
        LPG_TRY(
            save_expression(to, value->access_structure.object, whitespace));
        LPG_TRY(stream_writer_write_string(to, "."));
        expression const member =
            expression_from_identifier(value->access_structure.member);
        return save_expression(to, &member, whitespace);
    }

    case expression_type_match:
        LPG_TRY(space_here(to, &whitespace));
        LPG_TRY(stream_writer_write_string(to, "match "));
        LPG_TRY(save_expression(to, value->match.input, whitespace));
        LPG_TRY(stream_writer_write_string(to, "\n"));
        LPG_FOR(size_t, i, value->match.number_of_cases)
        {
            LPG_TRY(indent(to, go_deeper(whitespace, 1)));
            LPG_TRY(stream_writer_write_string(to, "case"));
            LPG_TRY(save_expression(
                to, value->match.cases[i].key,
                add_space_or_newline(go_deeper(whitespace, 2))));
            LPG_TRY(stream_writer_write_string(to, ":"));
            LPG_TRY(save_expression(
                to, value->match.cases[i].action,
                add_space_or_newline(go_deeper(whitespace, 2))));
            LPG_TRY(stream_writer_write_string(to, "\n"));
        }
        return success;

    case expression_type_string:
        LPG_TRY(space_here(to, &whitespace));
        LPG_TRY(stream_writer_write_string(to, "\""));
        LPG_FOR(size_t, i, value->string.value.length)
        {
            switch (value->string.value.data[i])
            {
            case '\"':
            case '\'':
            case '\\':
                LPG_TRY(stream_writer_write_string(to, "\\"));
                break;
            }
            LPG_TRY(stream_writer_write_bytes(
                to, (value->string.value.data + i), 1));
        }
        return stream_writer_write_string(to, "\"");

    case expression_type_identifier:
        LPG_TRY(space_here(to, &whitespace));
        return stream_writer_write_unicode_view(
            to, unicode_view_from_string(value->identifier.value));

    case expression_type_assign:
        LPG_TRY(save_expression(to, value->assign.left, whitespace));
        LPG_TRY(stream_writer_write_string(to, " = "));
        return save_expression(to, value->assign.right, whitespace);

    case expression_type_return:
        LPG_TRY(space_here(to, &whitespace));
        LPG_TRY(stream_writer_write_string(to, "return "));
        return save_expression(to, value->return_, whitespace);

    case expression_type_loop:
    {
        LPG_TRY(space_here(to, &whitespace));
        LPG_TRY(stream_writer_write_string(to, "loop"));
        expression const loop_body = expression_from_sequence(value->loop_body);
        return save_expression(to, &loop_body, go_deeper(whitespace, 1));
    }

    case expression_type_break:
        LPG_TRY(space_here(to, &whitespace));
        return stream_writer_write_string(to, "break");

    case expression_type_sequence:
        LPG_FOR(size_t, i, value->sequence.length)
        {
            /*we break the line here, so the space should not be generated
             * before the sequence element:*/
            whitespace.pending_space = 0;

            LPG_TRY(stream_writer_write_string(to, "\n"));
            LPG_TRY(indent(to, whitespace));
            LPG_TRY(
                save_expression(to, value->sequence.elements + i, whitespace));
        }
        return success;

    case expression_type_declare:
        LPG_TRY(stream_writer_write_string(to, "let "));
        LPG_TRY(stream_writer_write_unicode_view(
            to, unicode_view_from_string(value->declare.name.value)));
        if (value->declare.optional_type)
        {
            LPG_TRY(stream_writer_write_string(to, " : "));
            LPG_TRY(
                save_expression(to, value->declare.optional_type, whitespace));
        }
        LPG_TRY(stream_writer_write_string(to, " = "));
        LPG_TRY(save_expression(to, value->declare.initializer, whitespace));
        return success;

    case expression_type_tuple:
        LPG_TRY(space_here(to, &whitespace));
        LPG_TRY(stream_writer_write_string(to, "("));
        LPG_FOR(size_t, i, value->tuple.length)
        {
            if (i > 0)
            {
                LPG_TRY(stream_writer_write_string(to, ", "));
            }
            LPG_TRY(save_expression(to, value->tuple.elements + i, whitespace));
        }
        return stream_writer_write_string(to, ")");
    case expression_type_comment:
    {
        unicode_string comment_content = value->comment.value;
        if (unicode_string_find(comment_content, '\n').state == optional_set)
        {
            LPG_TRY(stream_writer_write_string(to, "/*"));
            LPG_TRY(stream_writer_write_unicode_view(
                to, unicode_view_from_string(comment_content)));
            return stream_writer_write_string(to, "*/");
        }
        LPG_TRY(stream_writer_write_string(to, "//"));
        return stream_writer_write_unicode_view(
            to, unicode_view_from_string(comment_content));
    }
    }
    UNREACHABLE();
}
