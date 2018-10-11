#pragma once
#include "lpg_create_process.h"
#include "lpg_unicode_view.h"

unicode_view path_remove_leaf(unicode_view const full) LPG_USE_RESULT;
unicode_string path_combine(unicode_view const *begin, size_t count) LPG_USE_RESULT;
unicode_string get_current_executable_path(void) LPG_USE_RESULT;
bool directory_exists(unicode_view const path) LPG_USE_RESULT;
bool file_exists(unicode_view const path) LPG_USE_RESULT;
success_indicator create_directory(unicode_view const path) LPG_USE_RESULT;
