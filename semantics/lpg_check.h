#pragma once
#include "lpg_type.h"
#include "lpg_parse_expression.h"
#include "lpg_checked_program.h"
#include "lpg_semantic_error.h"

typedef void check_error_handler(semantic_error, void *);

checked_program check(sequence const root, structure const global, LPG_NON_NULL(check_error_handler *on_error),
                      void *user);
