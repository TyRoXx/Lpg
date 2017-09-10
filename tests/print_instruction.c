#include "print_instruction.h"
#include "lpg_assert.h"
#include "lpg_instruction.h"
#include <stdio.h>

void print_value(value const printed)
{
    switch (printed.kind)
    {
    case value_kind_integer:
    {
        char buffer[64];
        buffer[sizeof(buffer) - 1] = '\0';
        char *const formatted =
            integer_format(printed.integer_, lower_case_digits, 10, buffer,
                           sizeof(buffer) - 1);
        printf("integer %s", formatted);
        break;
    }

    case value_kind_string:
        printf("string ?");
        break;

    case value_kind_function_pointer:
        printf("function pointer ?");
        break;

    case value_kind_flat_object:
        printf("flat object ?");
        break;

    case value_kind_type:
        printf("type ?");
        break;

    case value_kind_enum_element:
        printf("enum element # %u, state = (", printed.enum_element.which);
        print_value(printed.enum_element.state ? *printed.enum_element.state
                                               : value_from_unit());
        printf(")");
        break;

    case value_kind_unit:
        printf("unit");
        break;

    case value_kind_tuple:
        printf("tuple ");
        break;

    case value_kind_enum_constructor:
        LPG_TO_DO();
    }
}

void print_instruction(instruction const printed)
{
    switch (printed.type)
    {
    case instruction_call:
        printf("call callee %u result %u arguments", printed.call.callee,
               printed.call.result);
        for (size_t i = 0; i < printed.call.argument_count; ++i)
        {
            printf(" %u", printed.call.arguments[i]);
        }
        printf("\n");
        return;

    case instruction_loop:
        printf("loop\n");
        for (size_t i = 0; i < printed.loop.length; ++i)
        {
            printf("    ");
            print_instruction(printed.loop.elements[i]);
        }
        return;

    case instruction_global:
        printf("global\n");
        return;

    case instruction_read_struct:
        printf("read_struct from %u member %u into %u\n",
               printed.read_struct.from_object, printed.read_struct.member,
               printed.read_struct.into);
        return;

    case instruction_break:
        printf("break\n");
        return;

    case instruction_literal:
        printf("literal %u ", printed.literal.into);
        print_value(printed.literal.value_);
        printf("\n");
        return;

    case instruction_tuple:
        printf("tuple with length %zu\n", printed.tuple_.element_count);
        return;

    case instruction_enum_construct:
        LPG_TO_DO();
    }
    LPG_UNREACHABLE();
}

void print_instruction_sequence(instruction_sequence const sequence)
{
    for (size_t i = 0; i < sequence.length; ++i)
    {
        print_instruction(sequence.elements[i]);
    }
}
