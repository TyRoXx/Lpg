#include "test_save_statement.h"
#include "lpg_statement.h"
#include "test.h"
#include "lpg_stream_writer.h"
#include "lpg_save_expression.h"
#include "lpg_save_statement.h"
#include "lpg_allocate.h"

static void check_statement_rendering(statement tree, char const *expected)
{
    memory_writer buffer = {NULL, 0, 0};
    REQUIRE(save_statement(memory_writer_erase(&buffer), &tree, 0) == success);
    REQUIRE(memory_writer_equals(buffer, expected));
    memory_writer_free(&buffer);
    statement_free(&tree);
}

void test_save_statement(void)
{
    {
        check_statement_rendering(
            statement_from_assign(assign_create(
                expression_allocate(
                    expression_from_identifier(unicode_string_from_c_str("a"))),
                expression_allocate(
                    expression_from_integer_literal(integer_create(0, 123))))),
            "a = 123\n");
    }
    {
        check_statement_rendering(
            statement_from_return(expression_allocate(
                expression_from_identifier(unicode_string_from_c_str("a")))),
            "return a\n");
    }
    {
        check_statement_rendering(
            statement_from_expression(expression_allocate(
                expression_from_identifier(unicode_string_from_c_str("a")))),
            "a\n");
    }
    {
        check_statement_rendering(
            statement_from_loop(
                statement_allocate(statement_from_assign(assign_create(
                    expression_allocate(expression_from_identifier(
                        unicode_string_from_c_str("a"))),
                    expression_allocate(expression_from_integer_literal(
                        integer_create(0, 123))))))),
            "loop\n"
            "    a = 123\n");
    }
    {
        check_statement_rendering(
            statement_from_loop(statement_allocate(statement_from_loop(
                statement_allocate(statement_from_assign(assign_create(
                    expression_allocate(expression_from_identifier(
                        unicode_string_from_c_str("a"))),
                    expression_allocate(expression_from_integer_literal(
                        integer_create(0, 123))))))))),
            "loop\n"
            "    loop\n"
            "        a = 123\n");
    }
    {
        statement *body = allocate_array(2, sizeof(*body));
        body[0] = statement_from_assign(assign_create(
            expression_allocate(
                expression_from_identifier(unicode_string_from_c_str("a"))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 123)))));
        body[1] = statement_from_break();
        check_statement_rendering(
            statement_from_loop(statement_allocate(
                statement_from_sequence(sequence_create(body, 2)))),
            "loop\n"
            "    a = 123\n"
            "    break\n");
    }
    {
        statement *body = allocate_array(2, sizeof(*body));
        body[0] = statement_from_assign(assign_create(
            expression_allocate(
                expression_from_identifier(unicode_string_from_c_str("a"))),
            expression_allocate(
                expression_from_integer_literal(integer_create(0, 123)))));
        body[1] =
            statement_from_loop(statement_allocate(statement_from_break()));
        check_statement_rendering(
            statement_from_loop(statement_allocate(
                statement_from_sequence(sequence_create(body, 2)))),
            "loop\n"
            "    a = 123\n"
            "    loop\n"
            "        break\n");
    }
    {
        check_statement_rendering(statement_from_break(), "break\n");
    }
}
