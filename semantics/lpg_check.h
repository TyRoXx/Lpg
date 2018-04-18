#pragma once
#include "lpg_value.h"
#include "lpg_parse_expression.h"
#include "lpg_checked_program.h"
#include "lpg_semantic_error.h"
#include "lpg_program_check.h"

typedef void check_error_handler(semantic_error, void *);

checked_program check(sequence const root, structure const global, LPG_NON_NULL(check_error_handler *on_error),
                      LPG_NON_NULL(importer *import), void *user);
