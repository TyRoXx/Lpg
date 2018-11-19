#include "test_web.h"
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

void test_web(void)
{
    test_generate_main_html();
}
