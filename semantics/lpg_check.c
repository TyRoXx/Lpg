#include "lpg_check.h"
#include "lpg_for.h"
#include "lpg_assert.h"
#include "lpg_allocate.h"

structure structure_create(structure_member *members, struct_member_id count)
{
    structure result = {members, count};
    return result;
}

void structure_free(structure const *value)
{
    LPG_FOR(struct_member_id, i, value->count)
    {
        struct_member_free(value->members + i);
    }
    deallocate(value->members);
}

void type_free(type const *value)
{
    switch (value->kind)
    {
    case type_kind_structure:
        structure_free(&value->structure_);
        break;

    case type_kind_function_pointer:
        function_pointer_free(&value->function_pointer_);
        break;

    case type_kind_unit:
        break;
    }
}

void type_deallocate(type *value)
{
    type_free(value);
    deallocate(value);
}

type type_from_function_pointer(function_pointer value)
{
    type result;
    result.kind = type_kind_function_pointer;
    result.function_pointer_ = value;
    return result;
}

type type_from_unit(void)
{
    type result;
    result.kind = type_kind_unit;
    return result;
}

type *type_allocate(type const value)
{
    type *const result = allocate(sizeof(*result));
    *result = value;
    return result;
}

function_pointer function_pointer_create(type *result, type *arguments,
                                         size_t arity)
{
    function_pointer returned = {result, arguments, arity};
    return returned;
}

void function_pointer_free(function_pointer const *value)
{
    type_deallocate(value->result);
    LPG_FOR(size_t, i, value->arity)
    {
        type_free(value->arguments + i);
    }
    if (value->arguments)
    {
        deallocate(value->arguments);
    }
}

structure_member structure_member_create(type what, unicode_string name)
{
    structure_member result = {what, name};
    return result;
}

void struct_member_free(structure_member const *value)
{
    unicode_string_free(&value->name);
    type_free(&value->what);
}

static void add_instruction(instruction_sequence *to, instruction const added)
{
    to->elements =
        reallocate_array(to->elements, (to->length + 1), sizeof(*to->elements));
    to->elements[to->length] = added;
    ++(to->length);
}

struct function_checking_state;

typedef optional_register_id
read_variable_function(struct function_checking_state *, instruction_sequence *,
                       unicode_view, source_location);

typedef struct function_checking_state
{
    register_id used_registers;
    read_variable_function *read_variable;
    structure const *global;
    check_error_handler *on_error;
    void *user;
} function_checking_state;

static function_checking_state
function_checking_state_create(read_variable_function *read_variable,
                               structure const *global,
                               check_error_handler *on_error, void *user)
{
    function_checking_state result = {0, read_variable, global, on_error, user};
    return result;
}

static register_id allocate_register(function_checking_state *state)
{
    register_id id = state->used_registers;
    ++state->used_registers;
    return id;
}

static optional_register_id check_sequence(function_checking_state *state,
                                           instruction_sequence *output,
                                           sequence const input);

static optional_register_id evaluate_expression(function_checking_state *state,
                                                instruction_sequence *function,
                                                expression const element)
{
    switch (element.type)
    {
    case expression_type_lambda:
        LPG_TO_DO();

    case expression_type_call:
    {
        optional_register_id const callee =
            evaluate_expression(state, function, *element.call.callee);
        if (!callee.is_set)
        {
            return optional_register_id_empty;
        }
        register_id *const arguments =
            allocate_array(element.call.arguments.length, sizeof(*arguments));
        LPG_FOR(size_t, i, element.call.arguments.length)
        {
            optional_register_id argument = evaluate_expression(
                state, function, element.call.arguments.elements[i]);
            if (!argument.is_set)
            {
                return optional_register_id_empty;
            }
            arguments[i] = argument.value;
        }
        register_id const result = allocate_register(state);
        add_instruction(
            function, instruction_create_call(call_instruction_create(
                          callee.value, arguments,
                          element.call.arguments.length, result)));
        return optional_register_id_create(result);
    }

    case expression_type_integer_literal:
    case expression_type_access_structure:
    case expression_type_match:
    case expression_type_string:
        LPG_TO_DO();

    case expression_type_identifier:
    {
        unicode_view const name =
            unicode_view_from_string(element.identifier.value);
        optional_register_id address = state->read_variable(
            state, function, name, element.identifier.source);
        return address;
    }

    case expression_type_make_identifier:
    case expression_type_assign:
    case expression_type_return:
        LPG_TO_DO();

    case expression_type_loop:
    {
        instruction_sequence body = {NULL, 0};
        optional_register_id const result =
            check_sequence(state, &body, element.loop_body);
        add_instruction(function, instruction_create_loop(body));
        return result;
    }

    case expression_type_break:
    case expression_type_sequence:
    case expression_type_declare:
    case expression_type_tuple:
        LPG_TO_DO();
    }
    UNREACHABLE();
}

static optional_register_id check_sequence(function_checking_state *state,
                                           instruction_sequence *output,
                                           sequence const input)
{
    if (input.length == 0)
    {
        optional_register_id const final_result =
            optional_register_id_create(allocate_register(state));
        add_instruction(output, instruction_create_unit(final_result.value));
        return final_result;
    }
    optional_register_id final_result = optional_register_id_empty;
    LPG_FOR(size_t, i, input.length)
    {
        final_result = evaluate_expression(state, output, input.elements[i]);
    }
    return final_result;
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

optional_register_id optional_register_id_create(register_id value)
{
    optional_register_id result = {1, value};
    return result;
}

call_instruction call_instruction_create(register_id callee,
                                         register_id *arguments,
                                         size_t argument_count,
                                         register_id result)
{
    call_instruction created = {callee, arguments, argument_count, result};
    return created;
}

void call_instruction_free(call_instruction const *value)
{
    if (value->arguments)
    {
        deallocate(value->arguments);
    }
}

read_struct_instruction read_struct_instruction_create(register_id from_object,
                                                       struct_member_id member,
                                                       register_id into)
{
    read_struct_instruction result = {from_object, member, into};
    return result;
}

int read_struct_instruction_equals(read_struct_instruction const left,
                                   read_struct_instruction const right)
{
    return (left.from_object == right.from_object) &&
           (left.member == right.member) && (left.into == right.into);
}

instruction instruction_create_call(call_instruction argument)
{
    instruction result;
    result.type = instruction_call;
    result.call = argument;
    return result;
}

instruction instruction_create_global(register_id into)
{
    instruction result;
    result.type = instruction_global;
    result.global_into = into;
    return result;
}

instruction instruction_create_read_struct(read_struct_instruction argument)
{
    instruction result;
    result.type = instruction_read_struct;
    result.read_struct = argument;
    return result;
}

instruction instruction_create_loop(instruction_sequence body)
{
    instruction result;
    result.type = instruction_loop;
    result.loop = body;
    return result;
}

instruction instruction_create_unit(register_id into)
{
    instruction result;
    result.type = instruction_unit;
    result.unit = into;
    return result;
}

void instruction_free(instruction const *value)
{
    switch (value->type)
    {
    case instruction_call:
        call_instruction_free(&value->call);
        break;

    case instruction_loop:
        instruction_sequence_free(&value->loop);
        break;

    case instruction_global:
        break;

    case instruction_read_struct:
        break;

    case instruction_unit:
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

    case instruction_global:
        return (left.global_into == right.global_into);

    case instruction_read_struct:
        return read_struct_instruction_equals(
            left.read_struct, right.read_struct);

    case instruction_unit:
        return (left.unit == right.unit);
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

static optional_register_id read_variable(function_checking_state *state,
                                          instruction_sequence *to,
                                          unicode_view name,
                                          source_location where)
{
    structure const globals = *state->global;
    LPG_FOR(struct_member_id, i, globals.count)
    {
        if (unicode_view_equals(
                name, unicode_view_from_string(globals.members[i].name)))
        {
            register_id const global = allocate_register(state);
            add_instruction(to, instruction_create_global(global));
            register_id const result = allocate_register(state);
            add_instruction(
                to, instruction_create_read_struct(
                        read_struct_instruction_create(global, i, result)));
            return optional_register_id_create(result);
        }
    }
    state->on_error(where, state->user);
    return optional_register_id_empty;
}

checked_program check(sequence const root, structure const global,
                      check_error_handler *on_error, void *user)
{
    checked_program program = {
        allocate_array(1, sizeof(struct checked_function)), 1};
    program.functions[0].body = instruction_sequence_create(NULL, 0);
    function_checking_state state =
        function_checking_state_create(read_variable, &global, on_error, user);
    check_sequence(&state, &program.functions[0].body, root);
    return program;
}
