#include "lpg_interpret.h"
#include "lpg_for.h"
#include "lpg_assert.h"
#include "lpg_allocate.h"
#include "lpg_instruction.h"
#include <string.h>
#include "lpg_optional_function_id.h"
#include "lpg_standard_library.h"

static optional_value call_interpreted_function(checked_function const callee, optional_function_id const callee_id,
                                                optional_value const self, value *const arguments, value const *globals,
                                                value const *const captures, garbage_collector *const gc,
                                                checked_function const *const all_functions,
                                                lpg_interface const *const all_interfaces);

typedef enum run_sequence_result
{
    run_sequence_result_break = 1,
    run_sequence_result_continue,
    run_sequence_result_return,
    run_sequence_result_unavailable_at_this_time
} run_sequence_result;

optional_value call_function(function_pointer_value const callee, function_call_arguments const arguments)
{
    ASSUME(arguments.globals);
    ASSUME(arguments.gc);
    ASSUME(arguments.all_functions);
    if (callee.external)
    {
        value const return_value = callee.external(arguments, callee.captures, callee.external_environment);
        if (!callee.external_signature.result.is_set)
        {
            LPG_TO_DO();
        }
        ASSUME(value_conforms_to_type(return_value, callee.external_signature.result.value));
        return optional_value_create(return_value);
    }
    return call_interpreted_function(arguments.all_functions[callee.code], optional_function_id_create(callee.code),
                                     arguments.self, arguments.arguments, arguments.globals, callee.captures,
                                     arguments.gc, arguments.all_functions, arguments.all_interfaces);
}

typedef struct invoke_method_parameters
{
    function_id method;
    size_t parameter_count;
} invoke_method_parameters;

static value invoke_method(function_call_arguments const arguments, value const *const captures, void *environment)
{
    (void)environment;
    ASSUME(captures);
    value const from = captures[0];
    invoke_method_parameters const *const parameters = environment;
    switch (from.kind)
    {
    case value_kind_generic_struct:
        LPG_TO_DO();

    case value_kind_generic_lambda:
        LPG_TO_DO();

    case value_kind_type_erased:
    {
        implementation *const impl = implementation_ref_resolve(arguments.all_interfaces, from.type_erased.impl);
        ASSUME(impl);
        ASSUME(parameters->method < impl->method_count);
        size_t const method_parameter_count = parameters->parameter_count;
        function_pointer_value const *const function = &impl->methods[parameters->method];
        ASSUME(method_parameter_count < SIZE_MAX);
        ASSUME(!arguments.self.is_set);
        optional_value const result = call_function(
            *function, function_call_arguments_create(optional_value_create(*from.type_erased.self),
                                                      arguments.arguments, arguments.globals, arguments.gc,
                                                      arguments.all_functions, arguments.all_interfaces));
        if (!result.is_set)
        {
            LPG_TO_DO();
        }
        return result.value_;
    }

    case value_kind_array:
    {
        switch (parameters->method)
        {
        case 0: // size
            return value_from_integer(integer_create(0, from.array->count));

        case 1: // load
        {
            value const index = arguments.arguments[0];
            ASSUME(index.kind == value_kind_integer);
            if (integer_less(index.integer_, integer_create(0, from.array->count)))
            {
                value *const state = garbage_collector_allocate(arguments.gc, sizeof(*state));
                *state = from.array->elements[index.integer_.low];
                return value_from_enum_element(0, from.array->element_type, state);
            }
            return value_from_enum_element(1, type_from_unit(), NULL);
        }

        case 2: // store
        {
            value const index = arguments.arguments[0];
            ASSUME(index.kind == value_kind_integer);
            if (!integer_less(index.integer_, integer_create(0, from.array->count)))
            {
                return value_from_enum_element(0, type_from_unit(), NULL);
            }
            value const new_element = arguments.arguments[1];
            from.array->elements[index.integer_.low] = new_element;
            return value_from_enum_element(1, type_from_unit(), NULL);
        }

        case 3: // append
        {
            value const new_element = arguments.arguments[0];
            if (from.array->count == from.array->allocated)
            {
                size_t new_capacity = (from.array->count * 2);
                if (new_capacity == 0)
                {
                    new_capacity = 1;
                }
                value *const new_elements =
                    garbage_collector_allocate_array(arguments.gc, new_capacity, sizeof(*new_elements));
                if (from.array->count > 0)
                {
                    memcpy(new_elements, from.array->elements, sizeof(*new_elements) * from.array->count);
                }
                from.array->elements = new_elements;
                from.array->allocated = new_capacity;
            }
            from.array->elements[from.array->count] = new_element;
            from.array->count += 1;
            return value_from_enum_element(1, type_from_unit(), NULL);
        }

        default:
            LPG_UNREACHABLE();
        }
    }

    case value_kind_integer:
    case value_kind_string:
    case value_kind_function_pointer:
    case value_kind_structure:
    case value_kind_type:
    case value_kind_enum_element:
    case value_kind_unit:
    case value_kind_tuple:
    case value_kind_enum_constructor:
    case value_kind_pattern:
    case value_kind_generic_enum:
    case value_kind_generic_interface:
        LPG_UNREACHABLE();
    }
    LPG_UNREACHABLE();
}

static run_sequence_result run_sequence(instruction_sequence const sequence, value *const return_value,
                                        value const *globals, value *const registers, structure_value const captures,
                                        garbage_collector *const gc, checked_function const *const all_functions,
                                        lpg_interface const *const all_interfaces,
                                        optional_function_id const current_function)
{
    LPG_FOR(size_t, i, sequence.length)
    {
        instruction const element = sequence.elements[i];
        switch (element.type)
        {
        case instruction_new_array:
        {
            array_value *const array = garbage_collector_allocate(gc, sizeof(*array));
            *array = array_value_create(NULL, 0, 0, element.new_array.element_type);
            registers[element.new_array.into] = value_from_array(array);
            break;
        }

        case instruction_get_method:
        {
            size_t const capture_count = 1;
            value *const pseudo_captures =
                garbage_collector_allocate_array(gc, capture_count, sizeof(*pseudo_captures));
            ASSUME(value_is_valid(registers[element.get_method.from]));
            pseudo_captures[0] = registers[element.get_method.from];
            lpg_interface const *const interface_ = &all_interfaces[element.get_method.interface_];
            ASSUME(element.get_method.method < interface_->method_count);
            method_description const method = interface_->methods[element.get_method.method];
            invoke_method_parameters *const parameters = garbage_collector_allocate(gc, sizeof(*parameters));
            parameters->method = element.get_method.method;
            parameters->parameter_count = method.parameters.length;
            type *const pseudo_capture_types =
                garbage_collector_allocate_array(gc, capture_count, sizeof(*pseudo_capture_types));
            pseudo_capture_types[0] = type_from_interface(element.get_method.interface_);
            registers[element.get_method.into] = value_from_function_pointer(function_pointer_value_from_external(
                invoke_method, parameters, pseudo_captures,
                function_pointer_create(optional_type_create_set(method.result), method.parameters,
                                        tuple_type_create(pseudo_capture_types, capture_count),
                                        optional_type_create_empty())));
            break;
        }

        case instruction_erase_type:
        {
            value *const self = garbage_collector_allocate(gc, sizeof(*self));
            *self = registers[element.erase_type.self];
            registers[element.erase_type.into] =
                value_from_type_erased(type_erased_value_create(element.erase_type.impl, self));
            break;
        }

        case instruction_call:
        {
            value const callee = registers[element.call.callee];
            if (callee.kind == value_kind_unit)
            {
                return run_sequence_result_unavailable_at_this_time;
            }
            value *const arguments = allocate_array(element.call.argument_count, sizeof(*arguments));
            LPG_FOR(size_t, j, element.call.argument_count)
            {
                arguments[j] = registers[element.call.arguments[j]];
            }
            switch (callee.kind)
            {
            case value_kind_function_pointer:
            {
                optional_value const result = call_function(
                    callee.function_pointer, function_call_arguments_create(optional_value_empty, arguments, globals,
                                                                            gc, all_functions, all_interfaces));
                deallocate(arguments);
                if (!result.is_set)
                {
                    return run_sequence_result_unavailable_at_this_time;
                }
                ASSUME(value_is_valid(result.value_));
                registers[element.call.result] = result.value_;
                break;
            }

            case value_kind_type_erased:
            case value_kind_integer:
            case value_kind_string:
            case value_kind_structure:
            case value_kind_type:
            case value_kind_enum_element:
            case value_kind_unit:
            case value_kind_tuple:
            case value_kind_pattern:
                LPG_UNREACHABLE();

            case value_kind_enum_constructor:
            case value_kind_generic_enum:
            case value_kind_generic_interface:
            case value_kind_generic_struct:
            case value_kind_generic_lambda:
            case value_kind_array:
                LPG_TO_DO();
            }
            break;
        }

        case instruction_return:
            *return_value = registers[element.return_.returned_value];
            ASSUME(value_is_valid(*return_value));
            registers[element.return_.unit_goes_into] = value_from_unit();
            return run_sequence_result_return;

        case instruction_loop:
        {
            bool is_running = true;
            registers[element.loop.unit_goes_into] = value_from_unit();
            while (is_running)
            {
                switch (run_sequence(element.loop.body, return_value, globals, registers, captures, gc, all_functions,
                                     all_interfaces, current_function))
                {
                case run_sequence_result_break:
                    is_running = false;
                    break;

                case run_sequence_result_continue:
                    break;

                case run_sequence_result_unavailable_at_this_time:
                    return run_sequence_result_unavailable_at_this_time;

                case run_sequence_result_return:
                    return run_sequence_result_return;
                }
            }
            break;
        }

        case instruction_global:
            registers[element.global_into] =
                value_from_structure(structure_value_create(globals, standard_library_element_count));
            break;

        case instruction_read_struct:
        {
            structure_value const from = registers[element.read_struct.from_object].structure;
            ASSUME(element.read_struct.member < from.count);
            value const member = from.members[element.read_struct.member];
            ASSUME(value_is_valid(member));
            registers[element.read_struct.into] = member;
            break;
        }

        case instruction_break:
            registers[element.break_into] = value_from_unit();
            return run_sequence_result_break;

        case instruction_literal:
            ASSUME(value_is_valid(element.literal.value_));
            registers[element.literal.into] = element.literal.value_;
            break;

        case instruction_tuple:
        {
            value new_tuple;
            new_tuple.kind = value_kind_tuple;
            value *values = garbage_collector_allocate_array(gc, element.tuple_.element_count, sizeof(*values));
            for (size_t j = 0; j < element.tuple_.element_count; ++j)
            {
                values[j] = registers[*(element.tuple_.elements + j)];
                ASSUME(value_is_valid(values[j]));
            }
            new_tuple.tuple_ = value_tuple_create(values, element.tuple_.element_count);
            registers[element.tuple_.result] = new_tuple;
            break;
        }

        case instruction_instantiate_struct:
        {
            value *const values =
                garbage_collector_allocate_array(gc, element.instantiate_struct.argument_count, sizeof(*values));
            for (size_t j = 0; j < element.instantiate_struct.argument_count; ++j)
            {
                values[j] = registers[*(element.instantiate_struct.arguments + j)];
                ASSUME(value_is_valid(values[j]));
            }
            registers[element.instantiate_struct.into] =
                value_from_structure(structure_value_create(values, element.instantiate_struct.argument_count));
            break;
        }

        case instruction_enum_construct:
        {
            value *const state = garbage_collector_allocate(gc, sizeof(*state));
            *state = registers[element.enum_construct.state];
            ASSUME(value_is_valid(*state));
            registers[element.enum_construct.into] =
                value_from_enum_element(element.enum_construct.which.which, element.enum_construct.state_type, state);
            break;
        }

        case instruction_match:
        {
            value const key = registers[element.match.key];
            ASSUME(value_is_valid(key));
            bool found_match = false;
            for (size_t j = 0; j < element.match.count; ++j)
            {
                match_instruction_case *const this_case = element.match.cases + j;
                bool matches = false;
                switch (this_case->kind)
                {
                case match_instruction_case_kind_stateful_enum:
                    ASSUME(key.kind == value_kind_enum_element);
                    matches = (key.enum_element.which == this_case->stateful_enum.element);
                    if (matches)
                    {
                        registers[this_case->stateful_enum.where] = value_or_unit(key.enum_element.state);
                    }
                    break;

                case match_instruction_case_kind_value:
                    matches = value_equals(registers[this_case->key_value], key);
                    break;
                }
                if (!matches)
                {
                    continue;
                }
                found_match = true;
                switch (run_sequence(this_case->action, return_value, globals, registers, captures, gc, all_functions,
                                     all_interfaces, current_function))
                {
                case run_sequence_result_break:
                    return run_sequence_result_break;

                case run_sequence_result_continue:
                    break;

                case run_sequence_result_unavailable_at_this_time:
                    return run_sequence_result_unavailable_at_this_time;

                case run_sequence_result_return:
                    return run_sequence_result_return;
                }
                if (this_case->value.is_set)
                {
                    ASSUME(value_is_valid(registers[this_case->value.value]));
                    ASSUME(value_conforms_to_type(registers[this_case->value.value], element.match.result_type));
                    registers[element.match.result] = registers[this_case->value.value];
                }
                break;
            }
            ASSUME(found_match);
            break;
        }

        case instruction_get_captures:
            registers[element.captures] = value_from_structure(captures);
            break;

        case instruction_lambda_with_captures:
        {
            value *const inner_captures = garbage_collector_allocate_array(
                gc, element.lambda_with_captures.capture_count, sizeof(*inner_captures));
            for (size_t j = 0; j < element.lambda_with_captures.capture_count; ++j)
            {
                value const capture = registers[element.lambda_with_captures.captures[j]];
                inner_captures[j] = capture;
                ASSUME(value_is_valid(capture));
            }
            registers[element.lambda_with_captures.into] =
                value_from_function_pointer(function_pointer_value_from_internal(
                    element.lambda_with_captures.lambda, inner_captures, element.lambda_with_captures.capture_count));
            break;
        }

        case instruction_current_function:
        {
            value *const inner_captures = garbage_collector_allocate_array(gc, captures.count, sizeof(*inner_captures));
            for (size_t j = 0; j < captures.count; ++j)
            {
                value const capture = captures.members[j];
                inner_captures[j] = capture;
                ASSUME(value_is_valid(capture));
            }
            ASSUME(current_function.is_set);
            registers[element.current_function.into] = value_from_function_pointer(
                function_pointer_value_from_internal(current_function.value, inner_captures, captures.count));
            break;
        }
        }
    }
    return run_sequence_result_continue;
}

optional_value call_checked_function(checked_function const callee, optional_function_id const callee_id,
                                     value const *const captures, function_call_arguments arguments)
{
    return call_interpreted_function(callee, callee_id, arguments.self, arguments.arguments, arguments.globals,
                                     captures, arguments.gc, arguments.all_functions, arguments.all_interfaces);
}

static optional_value call_interpreted_function(checked_function const callee, optional_function_id const callee_id,
                                                optional_value const self, value *const arguments, value const *globals,
                                                value const *const captures, garbage_collector *const gc,
                                                checked_function const *const all_functions,
                                                lpg_interface const *const all_interfaces)
{
    /*there has to be at least one register for the return value, even if it is
     * unit*/
    ASSUME(callee.number_of_registers >= 1);
    ASSUME(globals);
    ASSUME(gc);
    ASSUME(all_functions);
    value *const registers = allocate_array(callee.number_of_registers, sizeof(*registers));
    register_id next_register = 0;
    if (callee.signature->self.is_set)
    {
        ASSUME(self.is_set);
        registers[next_register] = self.value_;
        ++next_register;
    }
    else
    {
        ASSUME(!self.is_set);
    }
    for (size_t i = 0; i < callee.signature->parameters.length; ++i)
    {
        type const parameter = callee.signature->parameters.elements[i];
        ASSUME(value_conforms_to_type(arguments[i], parameter));
        registers[next_register] = arguments[i];
        ++next_register;
    }
    value return_value = value_from_unit();
    switch (run_sequence(callee.body, &return_value, globals, registers,
                         structure_value_create(captures, callee.signature->captures.length), gc, all_functions,
                         all_interfaces, callee_id))
    {
    case run_sequence_result_break:
        LPG_UNREACHABLE();

    case run_sequence_result_continue:
    case run_sequence_result_return:
        deallocate(registers);
        if (!callee.signature->result.is_set)
        {
            LPG_TO_DO();
        }
        ASSUME(value_conforms_to_type(return_value, callee.signature->result.value));
        return optional_value_create(return_value);

    case run_sequence_result_unavailable_at_this_time:
        deallocate(registers);
        return optional_value_empty;
    }
    LPG_UNREACHABLE();
}

void interpret(checked_program const program, value const *globals, garbage_collector *const gc)
{
    function_id const entry_point_id = 0;
    call_interpreted_function(program.functions[entry_point_id], optional_function_id_create(entry_point_id),
                              optional_value_empty, NULL, globals, NULL, gc, program.functions, program.interfaces);
}
