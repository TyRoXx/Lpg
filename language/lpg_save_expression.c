#include "lpg_save_expression.h"
#include "lpg_assert.h"
#include "lpg_for.h"

success_indicator save_expression(stream_writer const to,
                                  expression const *value)
{
    switch (value->type)
    {
    case expression_type_lambda:
        LPG_TRY(stream_writer_write_string(to, "("));
        LPG_TRY(save_expression(to, value->lambda.parameter_name));
        LPG_TRY(stream_writer_write_string(to, ": "));
        LPG_TRY(save_expression(to, value->lambda.parameter_type));
        LPG_TRY(stream_writer_write_string(to, ") => "));
        LPG_TRY(save_expression(to, value->lambda.result));
        return success;

    case expression_type_builtin:
        switch (value->builtin)
        {
        case builtin_unit:
            return stream_writer_write_string(to, "()");

        case builtin_empty_structure:
            return stream_writer_write_string(to, "{}");

        case builtin_empty_variant:
            return stream_writer_write_string(to, "{}");
        }
        UNREACHABLE();

    case expression_type_call:
        LPG_TRY(save_expression(to, value->call.callee));
        LPG_TRY(stream_writer_write_string(to, "("));
        LPG_FOR(size_t, i, value->call.number_of_arguments)
        {
            if (i > 0)
            {
                LPG_TRY(stream_writer_write_string(to, ", "));
            }
            LPG_TRY(save_expression(to, &value->call.arguments[i]));
        }
        LPG_TRY(stream_writer_write_string(to, ")"));
        return success;

    case expression_type_local:
        UNREACHABLE();

    case expression_type_integer_literal:
    {
        char buffer[39];
        char *const formatted =
            integer_format(value->integer_literal, lower_case_digits, 10,
                           buffer, sizeof(buffer));
        return stream_writer_write_bytes(
            to, formatted, (size_t)(buffer + sizeof(buffer) - formatted));
    }

    case expression_type_integer_range:
    case expression_type_function:
    case expression_type_add_member:
    case expression_type_fill_structure:
    case expression_type_access_structure:
    case expression_type_add_to_variant:
    case expression_type_match:
    case expression_type_assignment:
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
    }
    UNREACHABLE();
}
