#pragma once

#include "lpg_path.h"
#include "lpg_array_size.h"

static unicode_string find_test_module_directory(void)
{
    unicode_view const pieces[] = {
        path_remove_leaf(unicode_view_from_c_str(__FILE__)), unicode_view_from_c_str("modules")};
    return path_combine(pieces, LPG_ARRAY_SIZE(pieces));
}
