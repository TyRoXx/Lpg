#include "test_c_backend.h"
#include "test.h"
#include "lpg_check.h"
#include "handle_parse_error.h"
#include <string.h>
#include "standard_library.h"
#include "lpg_c_backend.h"

static sequence parse(char const *input)
{
    test_parser_user user = {
        {input, strlen(input), source_location_create(0, 0)}, NULL, 0};
    expression_parser parser =
        expression_parser_create(find_next_token, handle_error, &user);
    sequence const result = parse_program(&parser);
    REQUIRE(user.base.remaining_size == 0);
    return result;
}

static void expect_no_errors(semantic_error const error, void *user)
{
    (void)error;
    (void)user;
    FAIL();
}

static void check_generated_c_code(char const *const source,
                                   structure const non_empty_global,
                                   char const *const expected_c)
{
    sequence root = parse(source);
    checked_program checked =
        check(root, non_empty_global, expect_no_errors, NULL);
    sequence_free(&root);
    REQUIRE(checked.function_count == 1);
    memory_writer generated = {NULL, 0, 0};
    REQUIRE(success == generate_c(checked, memory_writer_erase(&generated)));
    REQUIRE(memory_writer_equals(generated, expected_c));
    checked_program_free(&checked);
    memory_writer_free(&generated);
}

void test_c_backend(void)
{
    standard_library_description const std_library =
        describe_standard_library();

    check_generated_c_code("", std_library.globals, "#include <stdio.h>\n"
                                                    "int main(void)\n"
                                                    "{\n"
                                                    "    return 0;\n"
                                                    "}\n");

    check_generated_c_code("print(\"Hello, world!\")\n", std_library.globals,
                           "#include <stdio.h>\n"
                           "int main(void)\n"
                           "{\n"
                           "    fwrite(\"Hello, world!\", 1, 13, stdout);\n"
                           "    return 0;\n"
                           "}\n");

    check_generated_c_code("print(\"Hello, \")\nprint(\"world!\\n\")\n",
                           std_library.globals,
                           "#include <stdio.h>\n"
                           "int main(void)\n"
                           "{\n"
                           "    fwrite(\"Hello, \", 1, 7, stdout);\n"
                           "    fwrite(\"world!\\n\", 1, 7, stdout);\n"
                           "    return 0;\n"
                           "}\n");

    check_generated_c_code("loop\n"
                           "    print(\"Hello, world!\")\n"
                           "    break\n",
                           std_library.globals,
                           "#include <stdio.h>\n"
                           "int main(void)\n"
                           "{\n"
                           "    for (;;)\n"
                           "    {\n"
                           "        fwrite(\"Hello, world!\", 1, 13, stdout);\n"
                           "        break;\n"
                           "    }\n"
                           "    return 0;\n"
                           "}\n");

    check_generated_c_code("let s = concat(\"123\", \"456\")\n"
                           "print(s)\n",
                           std_library.globals,
                           "#include <stdio.h>\n"
                           "int main(void)\n"
                           "{\n"
                           "    fwrite(\"123456\", 1, 6, stdout);\n"
                           "    return 0;\n"
                           "}\n");

    standard_library_description_free(&std_library);
}
