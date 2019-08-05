#include "website.h"
#include "lpg_ecmascript_backend.h"
#include "lpg_read_file.h"
#include "lpg_write_file.h"
#include <stdio.h>
#include <string.h>

static optional_size find_template_marker(unicode_view const template)
{
    const size_t marker_length = strlen(web_template_marker);
    for (size_t i = 0; i <= template.length - marker_length; i++)
    {
        const char *index = template.begin + i;
        if (!memcmp(index, web_template_marker, marker_length))
        {
            return make_optional_size(i);
        }
    }
    return optional_size_empty;
}

success_indicator generate_main_html(unicode_view program, unicode_view const template, stream_writer const destination)
{
    const optional_size marker_point = find_template_marker(template);
    if (marker_point.state == optional_empty)
    {
        return success_no;
    }

    LPG_TRY(
        stream_writer_write_unicode_view(destination, unicode_view_create(template.begin, marker_point.value_if_set)));

    LPG_TRY(stream_writer_write_unicode_view(destination, program));

    const size_t offset = marker_point.value_if_set + strlen(web_template_marker);
    LPG_TRY(stream_writer_write_unicode_view(
        destination, unicode_view_create(template.begin + offset, template.length - offset)));
    return success_yes;
}

static unicode_string generate_ecmascript_code(checked_program const program)
{
    memory_writer buffer = {NULL, 0, 0};
    stream_writer writer = memory_writer_erase(&buffer);

    stream_writer_write_string(writer, "<script type=\"text/javascript\">"
                                       "\"use strict\";\n"
                                       "var assert = function (condition) { if "
                                       "(!condition) { alert(\"Assertion failed\"); } }\n"
                                       "var main = ");
    enum_encoding_strategy_cache strategy_cache =
        enum_encoding_strategy_cache_create(program.enums, program.enum_count);
    unicode_string const builtins = load_ecmascript_builtins();
    generate_ecmascript(program, &strategy_cache, unicode_view_from_string(builtins), &buffer);
    unicode_string_free(&builtins);

    {
        lpg_interface const *const host = get_host_interface(program);
        if (host)
        {
            generate_host_class(&strategy_cache, host, program.interfaces, writer);
            stream_writer_write_string(writer, "main(window, new Host());\n");
        }
        stream_writer_write_string(writer, "</script>");
    }

    enum_encoding_strategy_cache_free(strategy_cache);
    unicode_string result = {buffer.data, buffer.used};
    return result;
}

success_indicator generate_ecmascript_web_site(checked_program const program, unicode_view template,
                                               unicode_view const html_file)
{
    memory_writer buffer = {NULL, 0, 0};
    unicode_string generated_ecmascript = generate_ecmascript_code(program);
    success_indicator result =
        generate_main_html(unicode_view_from_string(generated_ecmascript), template, memory_writer_erase(&buffer));
    if (result == success_yes)
    {
        result = write_file(html_file, unicode_view_create(buffer.data, buffer.used));
    }

    unicode_string_free(&generated_ecmascript);
    memory_writer_free(&buffer);
    return result;
}

static unicode_string get_default_template(void)
{
    unicode_view first_part = unicode_view_from_c_str("<!DOCTYPE html><html>"
                                                      "<head>"
                                                      "<meta charset=\"UTF-8\">"
                                                      "<title>LPG web</title>"
                                                      "</head><body>");
    unicode_view last_part = unicode_view_from_c_str("</body></html>");

    const unicode_string end_of_string =
        unicode_view_concat(unicode_view_create(web_template_marker, strlen(web_template_marker)), last_part);

    unicode_string full_string = unicode_view_concat(first_part, unicode_view_from_string(end_of_string));
    unicode_string_free(&end_of_string);

    return full_string;
}

unicode_string generate_template(stream_writer diagnostics, const char *file_name)
{
    unicode_string new_string =
        unicode_view_concat(unicode_view_from_c_str(file_name), unicode_view_from_c_str(".html"));
    blob_or_error content = read_file(unicode_string_c_str(&new_string));
    unicode_string_free(&new_string);
    if (content.error)
    {
        stream_writer_write_string(diagnostics, content.error);
        stream_writer_write_string(diagnostics, "Could not find template. Using default template.\n");
        return get_default_template();
    }
    content.success.length = remove_carriage_returns(content.success.data, content.success.length);
    return unicode_string_validate(content.success);
}
