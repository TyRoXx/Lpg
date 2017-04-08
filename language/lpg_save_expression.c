#include "lpg_save_expression.h"
#include "lpg_assert.h"
#include "lpg_for.h"
#include "lpg_identifier.h"

static success_indicator indent(const stream_writer to,
                                size_t const indentation)
{
    LPG_FOR(size_t, j, indentation)
    {
        LPG_TRY(stream_writer_write_string(to, "    "));
    }
    return success;
}

success_indicator save_expression(stream_writer const to,
                                  expression const *value, size_t indentation)
{
    switch (value->type)
    {
    case expression_type_lambda:
        LPG_TRY(stream_writer_write_string(to, "("));
        LPG_FOR(size_t, i, value->lambda.parameter_count)
        {
            if (i > 0)
            {
                LPG_TRY(stream_writer_write_string(to, ", "));
            }
            parameter const *param = value->lambda.parameters + i;
            LPG_TRY(save_expression(to, param->name, indentation));
            LPG_TRY(stream_writer_write_string(to, ": "));
            LPG_TRY(save_expression(to, param->type, indentation));
        }
        LPG_TRY(stream_writer_write_string(to, ") => "));
        LPG_TRY(save_expression(to, value->lambda.result, indentation));
        return success;

    case expression_type_call:
    {
        LPG_TRY(save_expression(to, value->call.callee, indentation));
        expression const arguments =
            expression_from_tuple(value->call.arguments);
        return save_expression(to, &arguments, indentation);
    }

    case expression_type_integer_literal:
    {
        char buffer[39];
        char *const formatted =
            integer_format(value->integer_literal, lower_case_digits, 10,
                           buffer, sizeof(buffer));
        return stream_writer_write_bytes(
            to, formatted, (size_t)(buffer + sizeof(buffer) - formatted));
    }

    case expression_type_access_structure:
        LPG_TRY(
            save_expression(to, value->access_structure.object, indentation));
        LPG_TRY(stream_writer_write_string(to, "."));
        return save_expression(to, value->access_structure.member, indentation);

    case expression_type_match:
        LPG_TRY(stream_writer_write_string(to, "match "));
        LPG_TRY(save_expression(to, value->match.input, indentation));
        LPG_TRY(stream_writer_write_string(to, "\n"));
        LPG_FOR(size_t, i, value->match.number_of_cases)
        {
            LPG_TRY(indent(to, indentation + 1));
            LPG_TRY(stream_writer_write_string(to, "case "));
            LPG_TRY(save_expression(
                to, value->match.cases[i].key, indentation + 2));
            LPG_TRY(stream_writer_write_string(to, ": "));
            LPG_TRY(save_expression(
                to, value->match.cases[i].action, indentation + 2));
            LPG_TRY(stream_writer_write_string(to, "\n"));
        }
        return success;

    case expression_type_string:
        LPG_TRY(stream_writer_write_string(to, "\""));
        LPG_FOR(size_t, i, value->string.length)
        {
            switch (value->string.data[i])
            {
            case '\"':
            case '\'':
            case '\\':
                LPG_TRY(stream_writer_write_string(to, "\\"));
                break;
            }
            LPG_TRY(stream_writer_write_utf8(to, value->string.data[i]));
        }
        LPG_TRY(stream_writer_write_string(to, "\""));
        return success;

    case expression_type_identifier:
        LPG_FOR(size_t, i, value->identifier.length)
        {
            LPG_TRY(stream_writer_write_utf8(to, value->string.data[i]));
        }
        return success;

    case expression_type_make_identifier:
        LPG_TRY(stream_writer_write_string(to, "*"));
        return save_expression(to, value->make_identifier, indentation);

    case expression_type_assign:
        LPG_TRY(save_expression(to, value->assign.left, indentation));
        LPG_TRY(stream_writer_write_string(to, " = "));
        return save_expression(to, value->assign.right, indentation);

    case expression_type_return:
        LPG_TRY(stream_writer_write_string(to, "return "));
        return save_expression(to, value->return_, indentation);

    case expression_type_loop:
    {
        LPG_TRY(stream_writer_write_string(to, "loop"));
        expression const loop_body = expression_from_sequence(value->loop_body);
        return save_expression(to, &loop_body, indentation + 1);
    }

    case expression_type_break:
        return stream_writer_write_string(to, "break");

    case expression_type_sequence:
        LPG_FOR(size_t, i, value->sequence.length)
        {
            LPG_TRY(stream_writer_write_string(to, "\n"));
            LPG_TRY(indent(to, indentation));
            LPG_TRY(
                save_expression(to, value->sequence.elements + i, indentation));
        }
        return success;

    case expression_type_declare:
        LPG_TRY(save_expression(to, value->declare.name, indentation));
        LPG_TRY(stream_writer_write_string(to, ": "));
        LPG_TRY(save_expression(to, value->declare.type, indentation));
        if (value->declare.optional_initializer)
        {
            LPG_TRY(stream_writer_write_string(to, " = "));
            LPG_TRY(save_expression(
                to, value->declare.optional_initializer, indentation));
        }
        return success;

    case expression_type_tuple:
        LPG_TRY(stream_writer_write_string(to, "("));
        LPG_FOR(size_t, i, value->tuple.length)
        {
            if (i > 0)
            {
                LPG_TRY(stream_writer_write_string(to, ", "));
            }
            LPG_TRY(
                save_expression(to, value->tuple.elements + i, indentation));
        }
        return stream_writer_write_string(to, ")");
    }
    UNREACHABLE();
}
