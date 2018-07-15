#include "lpg_checked_function.h"
#include "lpg_allocate.h"

checked_function checked_function_create(function_pointer *signature, instruction_sequence body,
                                         unicode_string *register_debug_names, register_id number_of_registers)
{
    ASSUME((number_of_registers > 0) || (register_debug_names == NULL));
    checked_function const result = {signature, body, register_debug_names, number_of_registers};
    return result;
}

void checked_function_free(checked_function const *function)
{
    for (register_id i = 0; i < function->number_of_registers; ++i)
    {
        unicode_string_free(function->register_debug_names + i);
    }
    if (function->number_of_registers > 0)
    {
        deallocate(function->register_debug_names);
    }
    else
    {
        ASSUME(!function->register_debug_names);
    }
    instruction_sequence_free(&function->body);
    function_pointer_free(function->signature);
    deallocate(function->signature);
}

type get_return_type(type const callee, checked_function const *const all_functions,
                     lpg_interface const *const all_interfaces)
{
    switch (callee.kind)
    {
    case type_kind_host_value:
    case type_kind_generic_lambda:
        LPG_TO_DO();

    case type_kind_lambda:
        return all_functions[callee.lambda.lambda].signature->result;

    case type_kind_function_pointer:
        return callee.function_pointer_->result;

    case type_kind_tuple:
    case type_kind_structure:
    case type_kind_unit:
    case type_kind_string:
    case type_kind_enumeration:
    case type_kind_type:
    case type_kind_integer_range:
    case type_kind_interface:
    case type_kind_generic_enum:
    case type_kind_generic_interface:
        LPG_TO_DO();

    case type_kind_method_pointer:
        return all_interfaces[callee.method_pointer.interface_].methods[callee.method_pointer.method_index].result;

    case type_kind_enum_constructor:
        return type_from_enumeration(callee.enum_constructor->enumeration);
    }
    LPG_UNREACHABLE();
}
