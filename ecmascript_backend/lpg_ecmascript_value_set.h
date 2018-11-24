#pragma once

#include "lpg_ecmascript_value.h"

typedef struct ecmascript_integer_range
{
    uint64_t first;
    uint64_t after_last;
} ecmascript_integer_range;

ecmascript_integer_range ecmascript_integer_range_create(uint64_t first, uint64_t last);
ecmascript_integer_range ecmascript_integer_range_create_empty(void);
ecmascript_integer_range ecmascript_integer_range_create_any(void);
bool ecmascript_integer_range_is_empty(ecmascript_integer_range const range);
bool ecmascript_integer_range_equals(ecmascript_integer_range const left, ecmascript_integer_range const right);
bool ecmascript_integer_range_merge_without_intersection(ecmascript_integer_range *const into,
                                                         ecmascript_integer_range const from);

typedef struct ecmascript_value_set
{
    ecmascript_integer_range integer;
    bool undefined;
    bool null;
    bool string;
    bool array;
    bool object;
    bool function;
    bool bool_true;
    bool bool_false;
} ecmascript_value_set;

ecmascript_value_set ecmascript_value_set_create(ecmascript_integer_range integer, bool undefined, bool null,
                                                 bool string, bool array, bool object, bool function, bool bool_true,
                                                 bool bool_false);
ecmascript_value_set ecmascript_value_set_create_empty(void);
ecmascript_value_set ecmascript_value_set_create_function(void);
ecmascript_value_set ecmascript_value_set_create_string(void);
ecmascript_value_set ecmascript_value_set_create_integer_range(ecmascript_integer_range range);
ecmascript_value_set ecmascript_value_set_create_array(void);
ecmascript_value_set ecmascript_value_set_create_undefined(void);
ecmascript_value_set ecmascript_value_set_create_from_value(ecmascript_value const from);
ecmascript_value_set ecmascript_value_set_create_bool(bool value);
ecmascript_value_set ecmascript_value_set_create_object(void);
ecmascript_value_set ecmascript_value_set_create_anything(void);
bool ecmascript_value_set_merge_without_intersection(ecmascript_value_set *into, ecmascript_value_set const other);
bool ecmascript_value_set_is_empty(ecmascript_value_set const checked);
bool ecmascript_value_set_equals(ecmascript_value_set const left, ecmascript_value_set const right);
