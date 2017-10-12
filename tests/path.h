#pragma once
#include "lpg_unicode_view.h"

unicode_view path_remove_leaf(unicode_view const full);
unicode_string path_combine(unicode_view const *begin, size_t count);
