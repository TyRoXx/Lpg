#include "lpg_check.h"
#include "lpg_for.h"
#include "lpg_assert.h"
#include "lpg_allocate.h"

static void add_instruction(instruction_sequence *to, instruction const added)
{
    to->elements =
        reallocate_array(to->elements, (to->length + 1), sizeof(*to->elements));
    to->elements[to->length] = added;
    ++(to->length);
}

static void check_sequence(instruction_sequence *output, sequence const input);

static void sequence_element(instruction_sequence *function,
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
        instruction_sequence body = {NULL, 0};
        check_sequence(&body, element.loop_body);
        add_instruction(function, instruction_create_loop(body));
        break;
    }

    case expression_type_break:
    case expression_type_sequence:
    case expression_type_declare:
    case expression_type_tuple:
        UNREACHABLE();
    }
}

static void check_sequence(instruction_sequence *output, sequence const input)
{
    LPG_FOR(size_t, i, input.length)
    {
        sequence_element(output, input.elements[i]);
    }
}

instruction_sequence instruction_sequence_create(instruction *elements,
                                                 size_t length)
{
    instruction_sequence result = {elements, length};
    return result;
}

void instruction_sequence_free(instruction_sequence const *value)
{
    LPG_FOR(size_t, i, value->length)
    {
        instruction_free(value->elements + i);
    }
    if (value->elements)
    {
        deallocate(value->elements);
    }
}

int instruction_sequence_equals(instruction_sequence const *left,
                                instruction_sequence const *right)
{
    if (left->length != right->length)
    {
        return 0;
    }
    LPG_FOR(size_t, i, left->length)
    {
        if (!instruction_equals(left->elements[i], right->elements[i]))
        {
            return 0;
        }
    }
    return 1;
}

instruction instruction_create_call(void)
{
    instruction result = {
        instruction_call, instruction_sequence_create(NULL, 0)};
    return result;
}

instruction instruction_create_loop(instruction_sequence body)
{
    instruction result = {instruction_loop, body};
    return result;
}

void instruction_free(instruction const *value)
{
    switch (value->type)
    {
    case instruction_call:
        break;

    case instruction_loop:
        instruction_sequence_free(&value->loop);
        break;
    }
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

    case instruction_loop:
        return instruction_sequence_equals(&left.loop, &right.loop);
    }
    UNREACHABLE();
}

void checked_function_free(checked_function const *function)
{
    instruction_sequence_free(&function->body);
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
    program.functions[0].body = instruction_sequence_create(NULL, 0);
    check_sequence(&program.functions[0].body, root);
    return program;
}
