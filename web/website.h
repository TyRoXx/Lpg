#pragma once

#include "lpg_checked_program.h"
#include "lpg_stream_writer.h"
#include "lpg_try.h"
#include "lpg_unicode_string.h"

static char const *const web_template_marker = "|LPG|";

unicode_string generate_template(stream_writer diagnostics, const char *file_name);

success_indicator generate_ecmascript_web_site(checked_program const program, unicode_view template,
                                               unicode_view const html_file);

success_indicator generate_main_html(unicode_view program, unicode_view const template,
                                     stream_writer const destination);
