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

struct function_checking_state;

typedef struct evaluate_expression_result
{
    bool has_value;
    register_id where;
    type const *type_;
} evaluate_expression_result;

static evaluate_expression_result
evaluate_expression_result_create(register_id const where,
                                  type const *const type_)
{
    ASSUME(type_);
    evaluate_expression_result result = {true, where, type_};
    return result;
}

static evaluate_expression_result const evaluate_expression_result_empty = {
    false, 0, NULL};

typedef evaluate_expression_result
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

static evaluate_expression_result check_sequence(function_checking_state *state,
                                                 instruction_sequence *output,
                                                 sequence const input);

static unicode_string decode_string_literal(unicode_view const literal)
{
    ASSUME(literal.length >= 2);
    return unicode_string_from_range(literal.begin + 1, literal.length - 2);
}

static identifier_expression const *
evaluate_compile_time_expression(expression const *evaluated)
{
    switch (evaluated->type)
    {
    case expression_type_lambda:
    case expression_type_call:
    case expression_type_integer_literal:
    case expression_type_access_structure:
    case expression_type_match:
    case expression_type_string:
        LPG_TO_DO();

    case expression_type_identifier:
        return &evaluated->identifier;

    case expression_type_make_identifier:
    case expression_type_assign:
    case expression_type_return:
    case expression_type_loop:
    case expression_type_break:
    case expression_type_sequence:
    case expression_type_declare:
    case expression_type_tuple:
        LPG_TO_DO();
    }
    UNREACHABLE();
}

static type const *get_return_type(type const callee)
{
    switch (callee.kind)
    {
    case type_kind_structure:
        LPG_TO_DO();

    case type_kind_function_pointer:
        return callee.function_pointer_.result;

    case type_kind_unit:
    case type_kind_string_ref:
    case type_kind_enumeration:
        LPG_TO_DO();

    case type_kind_referenced:
        return get_return_type(*callee.referenced);
    }
    UNREACHABLE();
}

static type const *read_structure_element(
    function_checking_state *state, instruction_sequence *function,
    structure const *const type, register_id const where,
    unicode_view const element_name,
    source_location const element_name_location, register_id const result)
{
    LPG_FOR(struct_member_id, i, type->count)
    {
        if (unicode_view_equals(
                element_name, unicode_view_from_string(type->members[i].name)))
        {
            add_instruction(function, instruction_create_read_struct(
                                          read_struct_instruction_create(
                                              where, i, result)));
            return &type->members[i].what;
        }
    }
    state->on_error(semantic_error_create(
                        semantic_error_unknown_element, element_name_location),
                    state->user);
    return NULL;
}

static evaluate_expression_result
evaluate_expression(function_checking_state *state,
                    instruction_sequence *function, expression const element);

static type const *read_element(function_checking_state *state,
                                instruction_sequence *function,
                                expression const object_tree,
                                const identifier_expression *const element,
                                register_id const result)
{
    size_t const previous_function_size = function->length;
    register_id const previous_used_registers = state->used_registers;
    evaluate_expression_result const object =
        evaluate_expression(state, function, object_tree);
    if (!object.has_value)
    {
        LPG_TO_DO();
    }

    type const *const actual_type = (object.type_->kind == type_kind_referenced)
                                        ? object.type_->referenced
                                        : object.type_;
    switch (actual_type->kind)
    {
    case type_kind_structure:
        return read_structure_element(
            state, function, &actual_type->structure_, object.where,
            unicode_view_from_string(element->value), element->source, result);

    case type_kind_function_pointer:
    case type_kind_unit:
    case type_kind_string_ref:
        LPG_TO_DO();

    case type_kind_enumeration:
    {
        function->length = previous_function_size;
        state->used_registers = previous_used_registers;
        enumeration const *const enum_ = &actual_type->enum_;
        LPG_FOR(enum_element_id, i, enum_->size)
        {
            if (unicode_view_equals(
                    unicode_view_from_string(element->value),
                    unicode_view_from_string(enum_->elements[i].name)))
            {
                add_instruction(
                    function,
                    instruction_create_instantiate_enum(
                        instantiate_enum_instruction_create(result, i)));
                return object.type_;
            }
        }
        state->on_error(semantic_error_create(
                            semantic_error_unknown_element, element->source),
                        state->user);
        return NULL;
    }

    case type_kind_referenced:
        UNREACHABLE();
    }
    UNREACHABLE();
}

static evaluate_expression_result
evaluate_expression(function_checking_state *state,
                    instruction_sequence *function, expression const element)
{
    switch (element.type)
    {
    case expression_type_lambda:
        LPG_TO_DO();

    case expression_type_call:
    {
        evaluate_expression_result const callee =
            evaluate_expression(state, function, *element.call.callee);
        if (!callee.has_value)
        {
            return evaluate_expression_result_empty;
        }
        register_id *const arguments =
            allocate_array(element.call.arguments.length, sizeof(*arguments));
        LPG_FOR(size_t, i, element.call.arguments.length)
        {
            evaluate_expression_result argument = evaluate_expression(
                state, function, element.call.arguments.elements[i]);
            if (!argument.has_value)
            {
                return evaluate_expression_result_empty;
            }
            arguments[i] = argument.where;
        }
        register_id const result = allocate_register(state);
        add_instruction(
            function, instruction_create_call(call_instruction_create(
                          callee.where, arguments,
                          element.call.arguments.length, result)));
        ASSUME(callee.type_);
        type const *const return_type = get_return_type(*callee.type_);
        if (!return_type)
        {
            LPG_TO_DO();
        }
        return evaluate_expression_result_create(result, return_type);
    }

    case expression_type_integer_literal:
        LPG_TO_DO();

    case expression_type_access_structure:
    {
        identifier_expression const *const member =
            evaluate_compile_time_expression(element.access_structure.member);
        if (!member)
        {
            LPG_TO_DO();
        }
        register_id const result = allocate_register(state);
        type const *const element_type = read_element(
            state, function, *element.access_structure.object, member, result);
        if (!element_type)
        {
            LPG_TO_DO();
        }
        return evaluate_expression_result_create(result, element_type);
    }

    case expression_type_match:
        LPG_TO_DO();

    case expression_type_string:
    {
        register_id const result = allocate_register(state);
        add_instruction(
            function,
            instruction_create_string_literal(string_literal_instruction_create(
                decode_string_literal(unicode_view_from_string(element.string)),
                result)));
        static type const string_type = {type_kind_string_ref, {{NULL, 0}}};
        return evaluate_expression_result_create(result, &string_type);
    }

    case expression_type_identifier:
    {
        unicode_view const name =
            unicode_view_from_string(element.identifier.value);
        evaluate_expression_result address = state->read_variable(
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
        evaluate_expression_result const result =
            check_sequence(state, &body, element.loop_body);
        add_instruction(function, instruction_create_loop(body));
        return result;
    }

    case expression_type_break:
        add_instruction(function, instruction_create_break());
        return evaluate_expression_result_empty;

    case expression_type_sequence:
    case expression_type_declare:
    case expression_type_tuple:
        LPG_TO_DO();
    }
    UNREACHABLE();
}

static evaluate_expression_result check_sequence(function_checking_state *state,
                                                 instruction_sequence *output,
                                                 sequence const input)
{
    if (input.length == 0)
    {
        static type const unit_type = {type_kind_unit, {{NULL, 0}}};
        evaluate_expression_result const final_result =
            evaluate_expression_result_create(
                allocate_register(state), &unit_type);
        add_instruction(output, instruction_create_unit(final_result.where));
        return final_result;
    }
    evaluate_expression_result final_result = evaluate_expression_result_empty;
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

bool instruction_sequence_equals(instruction_sequence const *left,
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

bool call_instruction_equals(call_instruction const left,
                             call_instruction const right)
{
    if (left.callee != right.callee)
    {
        return false;
    }
    if (left.result != right.result)
    {
        return false;
    }
    if (left.argument_count != right.argument_count)
    {
        return false;
    }
    LPG_FOR(size_t, i, left.argument_count)
    {
        if (left.arguments[i] != right.arguments[i])
        {
            return false;
        }
    }
    return true;
}

read_struct_instruction read_struct_instruction_create(register_id from_object,
                                                       struct_member_id member,
                                                       register_id into)
{
    read_struct_instruction result = {from_object, member, into};
    return result;
}

bool read_struct_instruction_equals(read_struct_instruction const left,
                                    read_struct_instruction const right)
{
    return (left.from_object == right.from_object) &&
           (left.member == right.member) && (left.into == right.into);
}

string_literal_instruction
string_literal_instruction_create(unicode_string value, register_id into)
{
    string_literal_instruction result = {value, into};
    return result;
}

void string_literal_instruction_free(string_literal_instruction const *value)
{
    unicode_string_free(&value->value);
}

bool string_literal_instruction_equals(string_literal_instruction const left,
                                       string_literal_instruction const right)
{
    return unicode_string_equals(left.value, right.value) &&
           (left.into == right.into);
}

instantiate_enum_instruction
instantiate_enum_instruction_create(register_id into, enum_element_id element)
{
    instantiate_enum_instruction result = {into, element};
    return result;
}

bool instantiate_enum_instruction_equals(
    instantiate_enum_instruction const left,
    instantiate_enum_instruction const right)
{
    return (left.into == right.into) && (left.element == right.element);
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

instruction instruction_create_string_literal(string_literal_instruction value)
{
    instruction result;
    result.type = instruction_string_literal;
    result.string_literal = value;
    return result;
}

instruction instruction_create_break()
{
    instruction result;
    result.type = instruction_break;
    return result;
}

instruction
instruction_create_instantiate_enum(instantiate_enum_instruction value)
{
    instruction result;
    result.type = instruction_instantiate_enum;
    result.instantiate_enum = value;
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

    case instruction_string_literal:
        string_literal_instruction_free(&value->string_literal);
        break;

    case instruction_break:
        break;

    case instruction_instantiate_enum:
        break;
    }
}

bool instruction_equals(instruction const left, instruction const right)
{
    if (left.type != right.type)
    {
        return false;
    }
    switch (left.type)
    {
    case instruction_call:
        return call_instruction_equals(left.call, right.call);

    case instruction_loop:
        return instruction_sequence_equals(&left.loop, &right.loop);

    case instruction_global:
        return (left.global_into == right.global_into);

    case instruction_read_struct:
        return read_struct_instruction_equals(
            left.read_struct, right.read_struct);

    case instruction_unit:
        return (left.unit == right.unit);

    case instruction_string_literal:
        return string_literal_instruction_equals(
            left.string_literal, right.string_literal);

    case instruction_break:
        return true;

    case instruction_instantiate_enum:
        return instantiate_enum_instruction_equals(
            left.instantiate_enum, right.instantiate_enum);
    }
    UNREACHABLE();
}

void checked_function_free(checked_function const *function)
{
    instruction_sequence_free(&function->body);
}

semantic_error semantic_error_create(semantic_error_type type,
                                     source_location where)
{
    semantic_error result = {type, where};
    return result;
}

bool semantic_error_equals(semantic_error const left,
                           semantic_error const right)
{
    return (left.type == right.type) &&
           source_location_equals(left.where, right.where);
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

static evaluate_expression_result read_variable(function_checking_state *state,
                                                instruction_sequence *to,
                                                unicode_view name,
                                                source_location where)
{
    structure const globals = *state->global;
    size_t const previous_function_size = to->length;
    register_id const previous_used_registers = state->used_registers;
    register_id const global = allocate_register(state);
    add_instruction(to, instruction_create_global(global));
    register_id const result = allocate_register(state);
    type const *const element_type = read_structure_element(
        state, to, &globals, global, name, where, result);
    if (!element_type)
    {
        to->length = previous_function_size;
        state->used_registers = previous_used_registers;
        return evaluate_expression_result_empty;
    }
    return evaluate_expression_result_create(result, element_type);
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
    program.functions[0].number_of_registers = state.used_registers;
    return program;
}
