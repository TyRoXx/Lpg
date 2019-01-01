#include "print_instruction.h"
#include "lpg_instruction.h"

static void print_integer(integer const printed)
{
    char buffer[64];
    buffer[sizeof(buffer) - 1] = '\0';
    unicode_view const formatted = integer_format(printed, lower_case_digits, 10, buffer, sizeof(buffer) - 1);
    printf("%s", formatted.begin);
}

static void print_type(type const printed)
{
    switch (printed.kind)
    {
    case type_kind_generic_struct:
    case type_kind_host_value:
    case type_kind_generic_lambda:
        LPG_TO_DO();

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
    case type_kind_string:
    case type_kind_tuple:
    case type_kind_type:
    case type_kind_enum_constructor:
    case type_kind_lambda:
    case type_kind_interface:
    case type_kind_method_pointer:
    case type_kind_generic_enum:
    case type_kind_generic_interface:
        LPG_TO_DO();
    }
}

void print_value(value const printed, size_t const indentation)
{
    switch (printed.kind)
    {
    case value_kind_generic_struct:
    case value_kind_generic_lambda:
        LPG_TO_DO();

    case value_kind_array:
        LPG_TO_DO();

    case value_kind_type_erased:
        printf("type erased (impl: ?, self: ");
        print_value(*printed.type_erased.self, indentation);
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

    case value_kind_structure:
        printf("structure");
        for (size_t i = 0; i < printed.structure.count; ++i)
        {
            printf("\n");
            print_value(printed.structure.members[i], indentation + 1);
        }
        break;

    case value_kind_type:
        print_type(printed.type_);
        break;

    case value_kind_enum_element:
        printf("enum element # %u, state = (", printed.enum_element.which);
        print_value(printed.enum_element.state ? *printed.enum_element.state : value_from_unit(), indentation);
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
    case value_kind_generic_interface:
        LPG_TO_DO();
    }
}

void print_instruction(instruction const printed, size_t const indentation)
{
    switch (printed.type)
    {
    case instruction_current_function:
    case instruction_new_array:
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
        printf("loop unit into %u\n", printed.loop.unit_goes_into);
        print_instruction_sequence(printed.loop.body, (indentation + 1));
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
        print_value(printed.literal.value_, indentation);
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
            case match_instruction_case_kind_default:
                LPG_TO_DO();

            case match_instruction_case_kind_stateful_enum:
                printf("case %u(let %u) {\n", printed.match.cases[i].stateful_enum.element,
                       printed.match.cases[i].stateful_enum.where);
                break;

            case match_instruction_case_kind_value:
                printf("case %u {\n", printed.match.cases[i].key_value);
                break;
            }
            print_instruction_sequence(printed.match.cases[i].action, (indentation + 1));
            if (printed.match.cases[i].value.is_set)
            {
                printf("return %u }\n", printed.match.cases[i].value.value);
            }
            else
            {
                printf("no return }\n");
            }
        }
        return;
    }
    LPG_UNREACHABLE();
}

void print_instruction_sequence(instruction_sequence const sequence, size_t const indentation)
{
    for (size_t i = 0; i < sequence.length; ++i)
    {
        for (size_t j = 0; j < indentation; ++j)
        {
            printf("    ");
        }
        print_instruction(sequence.elements[i], indentation);
    }
}
