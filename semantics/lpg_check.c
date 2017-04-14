#include "lpg_check.h"
#include "lpg_for.h"
#include "lpg_assert.h"
#include "lpg_allocate.h"

static void add_instruction(checked_function *function, instruction added)
{
    function->body = reallocate_array(
        function->body, function->size + 1, sizeof(*function->body));
    function->body[function->size] = added;
    ++(function->size);
}

static void check_sequence(checked_function *function, sequence const input);

static void sequence_element(checked_function *function,
                             expression const element)
{
    switch (element.type)
    {
    case expression_type_lambda:
        UNREACHABLE();

    case expression_type_call:
        add_instruction(function, instruction_create_call());
        break;

    case expression_type_integer_literal:
    case expression_type_access_structure:
    case expression_type_match:
    case expression_type_string:
    case expression_type_identifier:
    case expression_type_make_identifier:
    case expression_type_assign:
    case expression_type_return:
        UNREACHABLE();

    case expression_type_loop:
    {
        jump_address const top = (jump_address)function->size;
        check_sequence(function, element.loop_body);
        add_instruction(function, instruction_create_jump(top));
        break;
    }

    case expression_type_break:
    case expression_type_sequence:
    case expression_type_declare:
    case expression_type_tuple:
        UNREACHABLE();
    }
}

static void check_sequence(checked_function *function, sequence const input)
{
    LPG_FOR(size_t, i, input.length)
    {
        sequence_element(function, input.elements[i]);
    }
}

instruction instruction_create_call(void)
{
    instruction result = {instruction_call, 0};
    return result;
}

instruction instruction_create_jump(jump_address destination)
{
    instruction result = {instruction_jump, destination};
    return result;
}

int instruction_equals(instruction const left, instruction const right)
{
    if (left.type != right.type)
    {
        return 0;
    }
    switch (left.type)
    {
    case instruction_call:
        return 1;

    case instruction_jump:
        return (left.jump_destination == right.jump_destination);
    }
    UNREACHABLE();
}

void checked_function_free(checked_function const *function)
{
    if (function->body)
    {
        deallocate(function->body);
    }
}

void checked_program_free(checked_program const *program)
{
    LPG_FOR(size_t, i, program->function_count)
    {
        checked_function_free(program->functions + i);
    }
    if (program->functions)
    {
        deallocate(program->functions);
    }
}

checked_program check(sequence const root)
{
    checked_program program = {
        allocate_array(1, sizeof(struct checked_function)), 1};
    program.functions[0].body = NULL;
    program.functions[0].size = 0;
    check_sequence(&program.functions[0], root);
    return program;
}
