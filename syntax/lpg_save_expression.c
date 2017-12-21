#include "lpg_save_expression.h"
#include "lpg_assert.h"
#include "lpg_for.h"

static success_indicator indent(const stream_writer to, whitespace_state const whitespace)
{
    LPG_FOR(size_t, j, whitespace.indentation_depth)
    {
        LPG_TRY(stream_writer_write_string(to, "    "));
    }
    return success;
}

static whitespace_state go_deeper(whitespace_state const whitespace, size_t additional_indentation)
{
    whitespace_state const result = {whitespace.indentation_depth + additional_indentation, 0};
    return result;
}

static whitespace_state add_space_or_newline(whitespace_state const whitespace)
{
    whitespace_state const result = {whitespace.indentation_depth, 1};
    return result;
}

static success_indicator space_here(stream_writer const to, whitespace_state *whitespace)
{
    if (whitespace->pending_space)
    {
        whitespace->pending_space = 0;
        return stream_writer_write_string(to, " ");
    }
    return success;
}

static success_indicator save_tuple_elements(stream_writer const to, tuple const value, whitespace_state whitespace)
{
    LPG_FOR(size_t, i, value.length)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(to, ", "));
        }
        LPG_TRY(save_expression(to, value.elements + i, whitespace));
    }
    return success;
}

static success_indicator save_function_header(stream_writer const to, function_header_tree const header,
                                              whitespace_state const whitespace)
{
    LPG_TRY(stream_writer_write_string(to, "("));
    LPG_FOR(size_t, i, header.parameter_count)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(to, ", "));
        }
        parameter const *param = header.parameters + i;
        LPG_TRY(stream_writer_write_bytes(to, param->name.value.data, param->name.value.length));
        LPG_TRY(stream_writer_write_string(to, ": "));
        LPG_TRY(save_expression(to, param->type, whitespace));
    }
    LPG_TRY(stream_writer_write_string(to, ")"));
    if (header.return_type != NULL)
    {
        LPG_TRY(stream_writer_write_string(to, ": "));
        LPG_TRY(save_expression(to, header.return_type, whitespace));
    }
    return success;
}

success_indicator save_sequence(stream_writer const to, sequence const value, whitespace_state whitespace)
{
    LPG_FOR(size_t, i, value.length)
    {
        if (whitespace.pending_space)
        {
            whitespace.pending_space = 0;
            LPG_TRY(stream_writer_write_string(to, "\n"));
        }
        else if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(to, "\n"));
        }
        LPG_TRY(indent(to, whitespace));
        LPG_TRY(save_expression(to, value.elements + i, whitespace));
    }
    return success;
}

static success_indicator save_tuple_expression(stream_writer const to, tuple const value, whitespace_state whitespace)
{
    LPG_TRY(space_here(to, &whitespace));
    LPG_TRY(stream_writer_write_string(to, "{"));
    LPG_TRY(save_tuple_elements(to, value, whitespace));
    return stream_writer_write_string(to, "}");
}

success_indicator save_expression(stream_writer const to, expression const *value, whitespace_state whitespace)
{
    switch (value->type)
    {
    case expression_type_lambda:
        LPG_TRY(space_here(to, &whitespace));
        LPG_TRY(save_function_header(to, value->lambda.header, whitespace));
        LPG_TRY(save_expression(to, value->lambda.result, add_space_or_newline(whitespace)));
        return success;

    case expression_type_call:
    {
        LPG_TRY(space_here(to, &whitespace));
        LPG_TRY(save_expression(to, value->call.callee, whitespace));
        LPG_TRY(stream_writer_write_string(to, "("));
        LPG_TRY(save_tuple_elements(to, value->call.arguments, whitespace));
        return stream_writer_write_string(to, ")");
    }

    case expression_type_integer_literal:
    {
        LPG_TRY(space_here(to, &whitespace));
        char buffer[39];
        char *const formatted =
            integer_format(value->integer_literal.value, lower_case_digits, 10, buffer, sizeof(buffer));
        return stream_writer_write_bytes(to, formatted, (size_t)(buffer + sizeof(buffer) - formatted));
    }

    case expression_type_access_structure:
    {
        LPG_TRY(space_here(to, &whitespace));
        LPG_TRY(save_expression(to, value->access_structure.object, whitespace));
        LPG_TRY(stream_writer_write_string(to, "."));
        expression const member = expression_from_identifier(value->access_structure.member);
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
            LPG_TRY(save_expression(to, value->match.cases[i].key, add_space_or_newline(go_deeper(whitespace, 1))));
            LPG_TRY(stream_writer_write_string(to, ":"));
            LPG_TRY(save_expression(to, value->match.cases[i].action, add_space_or_newline(go_deeper(whitespace, 1))));
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
            LPG_TRY(stream_writer_write_bytes(to, (value->string.value.data + i), 1));
        }
        return stream_writer_write_string(to, "\"");

    case expression_type_identifier:
        LPG_TRY(space_here(to, &whitespace));
        return stream_writer_write_unicode_view(to, unicode_view_from_string(value->identifier.value));

    case expression_type_not:
        LPG_TRY(stream_writer_write_string(to, "!"))
        return save_expression(to, value->not.expr, whitespace);

    case expression_type_return:
        LPG_TRY(space_here(to, &whitespace));
        LPG_TRY(stream_writer_write_string(to, "return "));
        return save_expression(to, value->return_, whitespace);

    case expression_type_loop:
    {
        LPG_TRY(space_here(to, &whitespace));
        LPG_TRY(stream_writer_write_string(to, "loop"));
        expression const loop_body = expression_from_sequence(value->loop_body);
        return save_expression(to, &loop_body, whitespace);
    }

    case expression_type_break:
        LPG_TRY(space_here(to, &whitespace));
        return stream_writer_write_string(to, "break");

    case expression_type_sequence:
    {
        whitespace_state in_sequence = go_deeper(whitespace, 1);
        in_sequence.pending_space = true;
        return save_sequence(to, value->sequence, in_sequence);
    }

    case expression_type_declare:
        LPG_TRY(stream_writer_write_string(to, "let "));
        LPG_TRY(stream_writer_write_unicode_view(to, unicode_view_from_string(value->declare.name.value)));
        if (value->declare.optional_type)
        {
            LPG_TRY(stream_writer_write_string(to, " : "));
            LPG_TRY(save_expression(to, value->declare.optional_type, whitespace));
        }
        LPG_TRY(stream_writer_write_string(to, " = "));
        LPG_TRY(save_expression(to, value->declare.initializer, whitespace));
        return success;

    case expression_type_tuple:
        return save_tuple_expression(to, value->tuple, whitespace);

    case expression_type_comment:
    {
        unicode_view view = unicode_view_from_string(value->comment.value);
        if (unicode_view_find(view, '\n').state == optional_set)
        {
            LPG_TRY(stream_writer_write_string(to, "/*"));
            LPG_TRY(stream_writer_write_unicode_view(to, view));
            return stream_writer_write_string(to, "*/");
        }
        LPG_TRY(stream_writer_write_string(to, "//"));
        return stream_writer_write_unicode_view(to, view);
    }

    case expression_type_binary:
    {
        LPG_TRY(save_expression(to, value->binary.left, whitespace));
        char const *operator= "";
        switch (value->binary.comparator)
        {
        case less_than:
            operator= "<";
            break;

        case less_than_or_equals:
            operator= "<=";
            break;

        case equals:
            operator= "==";
            break;

        case greater_than:
            operator= ">";
            break;

        case greater_than_or_equals:
            operator= ">=";
            break;

        case not_equals:
            operator= "!=";
            break;
        }
        LPG_TRY(stream_writer_write_string(to, " "));
        LPG_TRY(stream_writer_write_string(to, operator));
        LPG_TRY(stream_writer_write_string(to, " "));
        return save_expression(to, value->binary.right, whitespace);
    }

    case expression_type_impl:
    {
        LPG_TRY(stream_writer_write_string(to, "impl "));
        LPG_TRY(save_expression(to, value->impl.interface, whitespace));
        LPG_TRY(stream_writer_write_string(to, " for "));
        LPG_TRY(save_expression(to, value->impl.self, whitespace));
        whitespace_state in_impl = go_deeper(whitespace, 1);
        for (size_t i = 0; i < value->impl.method_count; ++i)
        {
            LPG_TRY(stream_writer_write_string(to, "\n"));
            LPG_TRY(indent(to, in_impl));
            impl_expression_method const method = value->impl.methods[i];
            {
                expression const name = expression_from_identifier(method.name);
                LPG_TRY(save_expression(to, &name, in_impl));
            }
            LPG_TRY(save_function_header(to, method.header, in_impl));
            {
                expression const body = expression_from_sequence(method.body);
                LPG_TRY(save_expression(to, &body, in_impl));
            }
        }
        return success;
    }

    case expression_type_struct:
    {
        LPG_TRY(stream_writer_write_string(to, "struct"));
        whitespace_state in_struct = go_deeper(whitespace, 1);
        for (size_t i = 0; i < value->struct_.element_count; ++i)
        {
            LPG_TRY(stream_writer_write_string(to, "\n"));
            LPG_TRY(indent(to, in_struct));
            LPG_TRY(
                stream_writer_write_unicode_view(to, unicode_view_from_string(value->struct_.elements[i].name.value)));
            LPG_TRY(stream_writer_write_string(to, ": "));
            LPG_TRY(save_expression(to, &value->struct_.elements[i].type, whitespace));
        }
        return success;
    }

    case expression_type_interface:
    {
        LPG_TRY(stream_writer_write_string(to, "interface"));
        whitespace_state in_interface = go_deeper(whitespace, 1);
        for (size_t i = 0; i < value->interface.method_count; ++i)
        {
            LPG_TRY(stream_writer_write_string(to, "\n"));
            LPG_TRY(indent(to, in_interface));
            LPG_TRY(
                stream_writer_write_unicode_view(to, unicode_view_from_string(value->interface.methods[i].name.value)));
            LPG_TRY(save_function_header(to, value->interface.methods[i].header, whitespace));
        }
        return success;
    }

    case expression_type_instantiate_struct:
    {
        LPG_TRY(save_expression(to, value->instantiate_struct.type, whitespace));
        return save_tuple_expression(to, value->instantiate_struct.arguments, whitespace);
    }
    }
    LPG_UNREACHABLE();
}
