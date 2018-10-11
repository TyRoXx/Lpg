#pragma once
#include "lpg_try.h"
#include "lpg_unicode_view.h"

success_indicator write_file(unicode_view const path, unicode_view const content);

unicode_string write_temporary_file(char const *const content);
