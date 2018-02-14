#include "print_instruction.h"
#include "lpg_instruction.h"

static void print_integer(integer const value)
{
    char buffer[64];
    buffer[sizeof(buffer) - 1] = '\0';
    char *const formatted = integer_format(value, lower_case_digits, 10, buffer, sizeof(buffer) - 1);
    printf("%s", formatted);
}

static void print_type(type const printed)
{
    switch (printed.kind)
    {
    case type_kind_unit:
        printf("unit");
        break;

    case type_kind_enumeration:
        printf("enum ?");
        break;

    case type_kind_integer_range:
        printf("integer(");
        print_integer(printed.integer_range_.minimum);
        printf(", ");
        print_integer(printed.integer_range_.maximum);
        printf(")");
        break;

    case type_kind_structure:
    case type_kind_function_pointer:
    case type_kind_string_ref:
    case type_kind_tuple:
    case type_kind_type:
    case type_kind_enum_constructor:
    case type_kind_lambda:
    case type_kind_interface:
    case type_kind_method_pointer:
    case type_kind_generic_enum:
        LPG_TO_DO();
    }
}

void print_value(value const printed)
{
    switch (printed.kind)
    {
    case value_kind_type_erased:
        printf("type erased (impl: ?, self: ");
        print_value(*printed.type_erased.self);
        printf(")");
        break;

    case value_kind_integer:
        printf("integer ");
        print_integer(printed.integer_);
        break;

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
        print_type(printed.type_);
        break;

    case value_kind_enum_element:
        printf("enum element # %u, state = (", printed.enum_element.which);
        print_value(printed.enum_element.state ? *printed.enum_element.state : value_from_unit());
        printf(", ");
        print_type(printed.enum_element.state_type);
        printf(")");
        break;

    case value_kind_unit:
        printf("unit");
        break;

    case value_kind_tuple:
        printf("tuple");
        break;

    case value_kind_enum_constructor:
        printf("enum_constructor");
        break;

    case value_kind_pattern:
    case value_kind_generic_enum:
        LPG_TO_DO();
    }
}

void print_instruction(instruction const printed)
{
    switch (printed.type)
    {
    case instruction_instantiate_struct:
    case instruction_get_method:
    case instruction_erase_type:
    case instruction_enum_construct:
    case instruction_get_captures:
    case instruction_lambda_with_captures:
        LPG_TO_DO();

    case instruction_call:
        printf("call callee %u result %u arguments", printed.call.callee, printed.call.result);
        for (size_t i = 0; i < printed.call.argument_count; ++i)
        {
            printf(" %u", printed.call.arguments[i]);
        }
        printf("\n");
        return;

    case instruction_return:
        printf("return %u (unit goes into %u)\n", printed.return_.returned_value, printed.return_.unit_goes_into);
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
        printf("read_struct from %u member %u into %u\n", printed.read_struct.from_object, printed.read_struct.member,
               printed.read_struct.into);
        return;

    case instruction_break:
        printf("break %u\n", printed.break_into);
        return;

    case instruction_literal:
        printf("literal %u ", printed.literal.into);
        print_value(printed.literal.value_);
        printf("\n");
        return;

    case instruction_tuple:
        printf("tuple with length %zu\n", printed.tuple_.element_count);
        return;

    case instruction_match:
        printf("match %u, result %u\n", printed.match.key, printed.match.result);
        for (size_t i = 0; i < printed.match.count; ++i)
        {
            switch (printed.match.cases[i].kind)
            {
            case match_instruction_case_kind_stateful_enum:
                printf("case %u(let %u) {\n", printed.match.cases[i].stateful_enum.element,
                       printed.match.cases[i].stateful_enum.where);
                break;

            case match_instruction_case_kind_value:
                printf("case %u {\n", printed.match.cases[i].key_value);
                break;
            }
            print_instruction_sequence(printed.match.cases[i].action);
            printf("return %u }\n", printed.match.cases[i].value);
        }
        return;
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
