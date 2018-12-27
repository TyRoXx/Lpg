#include "test_web.h"
#include "find_test_web_directory.h"
#include "lpg_cli.h"
#include "lpg_unicode_string.h"
#include "website.h"

static void test_generate_main_html(void)
{
    unicode_view program_code = unicode_view_from_c_str("Hello");
    unicode_view const template = unicode_view_from_c_str("<html><title>|fake|<title>|LPG|</html>");

    memory_writer buffer = {NULL, 0, 0};
    stream_writer writer = memory_writer_erase(&buffer);
    generate_main_html(program_code, template, writer);

    REQUIRE(memory_writer_equals(buffer, "<html><title>|fake|<title>Hello</html>"));

    memory_writer_free(&buffer);
}

static void test_generate_with_customized_template(void)
{
    memory_writer buffer = {NULL, 0, 0};
    stream_writer writer = memory_writer_erase(&buffer);
    unicode_string filepath = find_test_web_test_directory(unicode_view_from_c_str("test.lpg"));

    unicode_string template_content = generate_template(writer, unicode_string_c_str(&filepath));
    REQUIRE(buffer.used == 0);

    unicode_view const expected = unicode_view_from_c_str("<!DOCTYPE html>\n"
                                                          "<html>\n"
                                                          "<head>\n"
                                                          "\t<title>Hello I am a custom template</title>\n"
                                                          "</head>\n"
                                                          "<body>\n"
                                                          "\tVery custom\n"
                                                          "</body>\n"
                                                          "</html>");
    REQUIRE(unicode_view_equals(unicode_view_from_string(template_content), expected));

    unicode_string_free(&filepath);
    unicode_string_free(&template_content);
    memory_writer_free(&buffer);
}

void test_web(void)
{
    test_generate_main_html();
    test_generate_with_customized_template();
}
