#pragma once

#include "lpg_array_size.h"
#include "lpg_path.h"

static unicode_string find_test_web_test_directory(unicode_view file_name)
{
    unicode_view const pieces[] = {
        path_remove_leaf(unicode_view_from_c_str(__FILE__)), unicode_view_from_c_str("web_templates"), file_name};
    return path_combine(pieces, LPG_ARRAY_SIZE(pieces));
}