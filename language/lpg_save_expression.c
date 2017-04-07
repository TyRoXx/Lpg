#include "lpg_save_expression.h"
#include "lpg_assert.h"
#include "lpg_for.h"
#include "lpg_identifier.h"

success_indicator save_expression(stream_writer const to,
                                  expression const *value, size_t indentation)
{
    switch (value->type)
    {
    case expression_type_lambda:
        LPG_TRY(stream_writer_write_string(to, "("));
        LPG_TRY(save_expression(to, value->lambda.parameter_name, indentation));
        LPG_TRY(stream_writer_write_string(to, ": "));
        LPG_TRY(save_expression(to, value->lambda.parameter_type, indentation));
        LPG_TRY(stream_writer_write_string(to, ") => "));
        LPG_TRY(save_expression(to, value->lambda.result, indentation));
        return success;

    case expression_type_call:
        LPG_TRY(save_expression(to, value->call.callee, indentation));
        LPG_TRY(stream_writer_write_string(to, "("));
        LPG_FOR(size_t, i, value->call.number_of_arguments)
        {
            if (i > 0)
            {
                LPG_TRY(stream_writer_write_string(to, ", "));
            }
            LPG_TRY(
                save_expression(to, &value->call.arguments[i], indentation));
        }
        LPG_TRY(stream_writer_write_string(to, ")"));
        return success;

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
        UNREACHABLE();

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
        LPG_TRY(save_expression(to, value->assign.right, indentation));
        return stream_writer_write_string(to, "\n");

    case expression_type_return:
        LPG_TRY(stream_writer_write_string(to, "return "));
        LPG_TRY(save_expression(to, value->return_, indentation));
        return stream_writer_write_string(to, "\n");

    case expression_type_loop:
        LPG_TRY(stream_writer_write_string(to, "loop\n"));
        LPG_FOR(size_t, i, (indentation + 1))
        {
            LPG_TRY(stream_writer_write_string(to, "    "));
        }
        return save_expression(to, value->loop_body, indentation + 1);

    case expression_type_break:
        return stream_writer_write_string(to, "break\n");

    case expression_type_sequence:
        LPG_FOR(size_t, i, value->sequence.length)
        {
            if (i > 0)
            {
                LPG_FOR(size_t, j, indentation)
                {
                    LPG_TRY(stream_writer_write_string(to, "    "));
                }
            }
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
