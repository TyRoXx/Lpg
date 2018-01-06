#include "lpg_c_backend.h"
#include "lpg_assert.h"
#include "lpg_instruction.h"
#include "lpg_allocate.h"
#include <string.h>
#include "lpg_structure_member.h"

typedef struct standard_library_usage
{
    bool using_string_ref;
    bool using_assert;
    bool using_unit;
    bool using_stdint;
    bool using_integer;
    bool using_boolean;
} standard_library_usage;

static void standard_library_usage_use_string_ref(standard_library_usage *usage)
{
    usage->using_string_ref = true;
}

typedef enum register_meaning
{
    register_meaning_nothing,
    register_meaning_global,
    register_meaning_variable,
    register_meaning_assert,
    register_meaning_string_equals,
    register_meaning_integer_equals,
    register_meaning_integer_less,
    register_meaning_integer_to_string,
    register_meaning_or,
    register_meaning_and,
    register_meaning_not,
    register_meaning_concat,
    register_meaning_argument,
    register_meaning_captures,
    register_meaning_capture,
    register_meaning_side_effect,
    register_meaning_unit
} register_meaning;

typedef enum register_resource_ownership
{
    register_resource_ownership_owns,
    register_resource_ownership_borrows
} register_resource_ownership;

typedef struct register_state
{
    register_meaning meaning;
    register_resource_ownership ownership;
    optional_type type_of;
    union
    {
        value literal;
        capture_index capture;
    };
} register_state;

typedef struct type_definition
{
    type what;
    unicode_string name;
    unicode_string definition;
    size_t order;
} type_definition;

static void type_definition_free(type_definition const definition)
{
    unicode_string_free(&definition.definition);
    unicode_string_free(&definition.name);
}

typedef struct type_definitions
{
    type_definition *elements;
    size_t count;
    size_t next_order;
} type_definitions;

static void type_definitions_free(type_definitions const definitions)
{
    for (size_t i = 0; i < definitions.count; ++i)
    {
        type_definition_free(definitions.elements[i]);
    }
    if (definitions.elements)
    {
        deallocate(definitions.elements);
    }
}

static size_t *type_definitions_sort_by_order(type_definitions const definitions)
{
    size_t *const result = allocate_array(definitions.count, sizeof(*result));
    for (size_t i = 0; i < definitions.count; ++i)
    {
        result[i] = ~(size_t)0;
    }
    for (size_t i = 0; i < definitions.count; ++i)
    {
        ASSUME(result[definitions.elements[i].order] == ~(size_t)0);
        result[definitions.elements[i].order] = i;
    }
    for (size_t i = 0; i < definitions.count; ++i)
    {
        ASSUME(result[i] != ~(size_t)0);
    }
    return result;
}

typedef struct c_backend_state
{
    register_state *registers;
    standard_library_usage standard_library;
    register_id *active_registers;
    size_t active_register_count;
    checked_function const *all_functions;
    interface const *all_interfaces;
    type_definitions *definitions;
    structure const *all_structs;
    enumeration const *all_enums;
} c_backend_state;

static void active_register(c_backend_state *const state, register_id const id)
{
    for (size_t i = 0; i < state->active_register_count; ++i)
    {
        ASSERT(state->active_registers[i] != id);
    }
    state->active_registers =
        reallocate_array(state->active_registers, (state->active_register_count + 1), sizeof(*state->active_registers));
    state->active_registers[state->active_register_count] = id;
    ++(state->active_register_count);
}

static void set_register_meaning(c_backend_state *const state, register_id const id, optional_type const type_of,
                                 register_meaning const meaning)
{
    ASSERT(state->registers[id].meaning == register_meaning_nothing);
    ASSERT(meaning != register_meaning_nothing);
    ASSERT(meaning != register_meaning_variable);
    state->registers[id].meaning = meaning;
    state->registers[id].ownership = register_resource_ownership_borrows;
    state->registers[id].type_of = type_of;
    active_register(state, id);
}

static void set_register_to_capture(c_backend_state *const state, register_id const id, capture_index const capture,
                                    type const type_of)
{
    ASSERT(state->registers[id].meaning == register_meaning_nothing);
    state->registers[id].meaning = register_meaning_capture;
    state->registers[id].capture = capture;
    state->registers[id].type_of = optional_type_create_set(type_of);
    active_register(state, id);
}

static void set_register_variable(c_backend_state *const state, register_id const id,
                                  register_resource_ownership ownership, type const type_of)
{
    ASSERT(state->registers[id].meaning == register_meaning_nothing);
    state->registers[id].meaning = register_meaning_variable;
    state->registers[id].ownership = ownership;
    state->registers[id].type_of = optional_type_create_set(type_of);
    active_register(state, id);
}

static void set_register_function_variable(c_backend_state *const state, register_id const id,
                                           register_resource_ownership ownership, type const type_of)
{
    ASSERT(state->registers[id].meaning == register_meaning_nothing);
    state->registers[id].meaning = register_meaning_variable;
    state->registers[id].ownership = ownership;
    state->registers[id].type_of = optional_type_create_set(type_of);
    active_register(state, id);
}

static void set_register_argument(c_backend_state *const state, register_id const id,
                                  register_resource_ownership const ownership, type const type_of)
{
    ASSERT(state->registers[id].meaning == register_meaning_nothing);
    state->registers[id].meaning = register_meaning_argument;
    state->registers[id].ownership = ownership;
    state->registers[id].type_of = optional_type_create_set(type_of);
    active_register(state, id);
}

static success_indicator encode_string_literal(unicode_view const content, stream_writer const c_output)
{
    LPG_TRY(stream_writer_write_string(c_output, "\""));
    for (size_t i = 0; i < content.length; ++i)
    {
        switch (content.begin[i])
        {
        case '\n':
            LPG_TRY(stream_writer_write_string(c_output, "\\n"));
            break;

        default:
            LPG_TRY(stream_writer_write_bytes(c_output, content.begin + i, 1));
        }
    }
    LPG_TRY(stream_writer_write_string(c_output, "\""));
    return success;
}

static success_indicator generate_integer(integer const value, stream_writer const c_output)
{
    char buffer[64];
    char const *const formatted = integer_format(value, lower_case_digits, 10, buffer, sizeof(buffer));
    LPG_TRY(stream_writer_write_bytes(c_output, formatted, (size_t)((buffer + sizeof(buffer)) - formatted)));
    return success;
}

static success_indicator escape_identifier(unicode_view const original, stream_writer const c_output)
{
    for (size_t i = 0; i < original.length; ++i)
    {
        if (original.begin[i] == '-')
        {
            LPG_TRY(stream_writer_write_string(c_output, "_"));
        }
        else
        {
            LPG_TRY(stream_writer_write_unicode_view(c_output, unicode_view_create(original.begin + i, 1)));
        }
    }
    return success;
}

static success_indicator generate_register_name(register_id const id, checked_function const *const current_function,
                                                stream_writer const c_output)
{
    unicode_view const original_name = unicode_view_from_string(current_function->register_debug_names[id]);
    if (original_name.length > 0)
    {
        LPG_TRY(escape_identifier(original_name, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "_"));
    }
    else
    {
        LPG_TRY(stream_writer_write_string(c_output, "r_"));
    }
    return generate_integer(integer_create(0, id), c_output);
}

static success_indicator generate_tuple_element_name(struct_member_id const element, stream_writer const c_output)
{
    LPG_TRY(stream_writer_write_string(c_output, "e_"));
    return generate_integer(integer_create(0, element), c_output);
}

static success_indicator generate_function_name(function_id const id, stream_writer const c_output)
{
    LPG_TRY(stream_writer_write_string(c_output, "lambda_"));
    char buffer[64];
    char const *const formatted = integer_format(integer_create(0, id), lower_case_digits, 10, buffer, sizeof(buffer));
    LPG_TRY(stream_writer_write_bytes(c_output, formatted, (size_t)((buffer + sizeof(buffer)) - formatted)));
    return success;
}

static unicode_string const *find_type_definition(type_definitions const definitions, type const needle)
{
    for (size_t i = 0; i < definitions.count; ++i)
    {
        if (type_equals(definitions.elements[i].what, needle))
        {
            return &definitions.elements[i].name;
        }
    }
    return NULL;
}

static unicode_string make_type_definition_name(size_t const index)
{
    char buffer[64];
    char *const formatted = integer_format(integer_create(0, index), lower_case_digits, 10, buffer, sizeof(buffer));
    size_t const index_length = (size_t)((buffer + sizeof(buffer)) - formatted);
    char const *const prefix = "type_definition_";
    size_t const name_length = strlen(prefix) + index_length;
    unicode_string const name = {allocate(name_length), name_length};
    memcpy(name.data, prefix, strlen(prefix));
    memcpy(name.data + strlen(prefix), formatted, index_length);
    return name;
}

static success_indicator indent(size_t const indentation, stream_writer const c_output)
{
    for (size_t i = 0; i < indentation; ++i)
    {
        LPG_TRY(stream_writer_write_string(c_output, "    "));
    }
    return success;
}

static success_indicator generate_type(type const generated, standard_library_usage *const standard_library,
                                       type_definitions *const definitions, checked_function const *const all_functions,
                                       interface const *const all_interfaces, enumeration const *const all_enums,
                                       stream_writer const c_output);

static success_indicator generate_c_function_pointer(type const generated,
                                                     standard_library_usage *const standard_library,
                                                     type_definitions *const definitions,
                                                     checked_function const *const all_functions,
                                                     interface const *const all_interfaces,
                                                     enumeration const *const all_enums, stream_writer const c_output)
{
    unicode_string const *const existing_definition = find_type_definition(*definitions, generated);
    if (existing_definition)
    {
        return stream_writer_write_unicode_view(c_output, unicode_view_from_string(*existing_definition));
    }
    memory_writer definition_buffer = {NULL, 0, 0};
    stream_writer definition_writer = memory_writer_erase(&definition_buffer);
    size_t const definition_index = definitions->count;
    ++(definitions->count);
    unicode_string name = make_type_definition_name(definition_index);
    definitions->elements =
        reallocate_array(definitions->elements, (definitions->count + 1), sizeof(*definitions->elements));
    {
        type_definition *const new_definition = definitions->elements + definition_index;
        new_definition->what = generated;
        new_definition->name = name;
    }
    LPG_TRY(stream_writer_write_string(definition_writer, "typedef "));
    LPG_TRY(generate_type(generated.function_pointer_->result, standard_library, definitions, all_functions,
                          all_interfaces, all_enums, definition_writer));
    LPG_TRY(stream_writer_write_string(definition_writer, " (*"));
    LPG_TRY(stream_writer_write_unicode_view(definition_writer, unicode_view_from_string(name)));
    LPG_TRY(stream_writer_write_string(definition_writer, ")("));
    for (size_t i = 0; i < generated.function_pointer_->parameters.length; ++i)
    {
        if (i > 0)
        {
            LPG_TRY(stream_writer_write_string(definition_writer, ", "));
        }
        LPG_TRY(generate_type(generated.function_pointer_->parameters.elements[i], standard_library, definitions,
                              all_functions, all_interfaces, all_enums, definition_writer));
    }
    LPG_TRY(stream_writer_write_string(definition_writer, ");\n"));
    type_definition *const new_definition = definitions->elements + definition_index;
    new_definition->definition.data = definition_buffer.data;
    new_definition->definition.length = definition_buffer.used;
    new_definition->order = definitions->next_order++;
    return stream_writer_write_unicode_view(c_output, unicode_view_from_string(name));
}

static success_indicator generate_interface_vtable_name(interface_id const generated, stream_writer const c_output)
{
    LPG_TRY(stream_writer_write_string(c_output, "interface_vtable_"));
    return stream_writer_write_integer(c_output, integer_create(0, generated));
}

static success_indicator
generate_interface_vtable_definition(interface_id const generated, standard_library_usage *const standard_library,
                                     type_definitions *const definitions, checked_function const *const all_functions,
                                     interface const *const all_interfaces, enumeration const *const all_enums,
                                     stream_writer const c_output)
{
    LPG_TRY(stream_writer_write_string(c_output, "typedef struct "));
    LPG_TRY(generate_interface_vtable_name(generated, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "\n"));
    LPG_TRY(stream_writer_write_string(c_output, "{\n"));

    LPG_TRY(indent(1, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "void (*_add_reference)(void *, ptrdiff_t);\n"));

    interface const our_interface = all_interfaces[generated];
    for (function_id i = 0; i < our_interface.method_count; ++i)
    {
        LPG_TRY(indent(1, c_output));
        method_description const method = our_interface.methods[i];
        LPG_TRY(generate_type(
            method.result, standard_library, definitions, all_functions, all_interfaces, all_enums, c_output));
        LPG_TRY(stream_writer_write_string(c_output, " (*"));
        LPG_TRY(escape_identifier(unicode_view_from_string(method.name), c_output));
        LPG_TRY(stream_writer_write_string(c_output, ")(void *"));
        for (size_t k = 0; k < method.parameters.length; ++k)
        {
            LPG_TRY(stream_writer_write_string(c_output, ", "));
            LPG_TRY(generate_type(method.parameters.elements[k], standard_library, definitions, all_functions,
                                  all_interfaces, all_enums, c_output));
        }
        LPG_TRY(stream_writer_write_string(c_output, ");\n"));
    }

    LPG_TRY(stream_writer_write_string(c_output, "} "));
    /*no newline here because clang-format will replace it with a
     * space*/

    LPG_TRY(generate_interface_vtable_name(generated, c_output));
    LPG_TRY(stream_writer_write_string(c_output, ";\n"));
    return success;
}

static success_indicator generate_interface_reference_name(interface_id const generated, stream_writer const c_output)
{
    LPG_TRY(stream_writer_write_string(c_output, "interface_reference_"));
    return stream_writer_write_integer(c_output, integer_create(0, generated));
}

static success_indicator generate_struct_name(struct_id const generated, stream_writer const c_output)
{
    LPG_TRY(stream_writer_write_string(c_output, "struct_"));
    return stream_writer_write_integer(c_output, integer_create(0, generated));
}

static success_indicator generate_interface_reference_definition(interface_id const generated,
                                                                 stream_writer const c_output)
{
    LPG_TRY(stream_writer_write_string(c_output, "typedef struct "));
    LPG_TRY(generate_interface_reference_name(generated, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "\n"));
    LPG_TRY(stream_writer_write_string(c_output, "{\n"));

    LPG_TRY(indent(1, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "struct "));
    LPG_TRY(generate_interface_vtable_name(generated, c_output));
    LPG_TRY(stream_writer_write_string(c_output, " const *vtable;\n"));

    LPG_TRY(indent(1, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "void *self;\n"));

    LPG_TRY(stream_writer_write_string(c_output, "} "));

    LPG_TRY(generate_interface_reference_name(generated, c_output));
    LPG_TRY(stream_writer_write_string(c_output, ";\n"));
    return success;
}

static success_indicator generate_interface_impl_name(implementation_ref const generated, stream_writer const c_output)
{
    LPG_TRY(stream_writer_write_string(c_output, "interface_impl_"));
    LPG_TRY(stream_writer_write_integer(c_output, integer_create(0, generated.implementation_index)));
    LPG_TRY(stream_writer_write_string(c_output, "_for_"));
    LPG_TRY(stream_writer_write_integer(c_output, integer_create(0, generated.target)));
    return success;
}

static success_indicator generate_interface_impl_add_reference(implementation_ref const generated,
                                                               stream_writer const c_output)
{
    LPG_TRY(stream_writer_write_string(c_output, "add_reference_"));
    LPG_TRY(stream_writer_write_integer(c_output, integer_create(0, generated.implementation_index)));
    LPG_TRY(stream_writer_write_string(c_output, "_for_"));
    LPG_TRY(stream_writer_write_integer(c_output, integer_create(0, generated.target)));
    return success;
}

static success_indicator generate_free(standard_library_usage *const standard_library, unicode_view const freed,
                                       type const what, checked_function const *const all_functions,
                                       structure const *const all_structures, enumeration const *const all_enums,
                                       size_t const indentation, stream_writer const c_output);

static success_indicator
generate_interface_impl_definition(implementation_ref const generated, type_definitions *const definitions,
                                   checked_function const *const all_functions, interface const *const all_interfaces,
                                   structure const *const all_structures, enumeration const *const all_enums,
                                   standard_library_usage *const standard_library, stream_writer const c_output)
{
    implementation_entry const impl = all_interfaces[generated.target].implementations[generated.implementation_index];

    LPG_TRY(stream_writer_write_string(c_output, "static void "));
    LPG_TRY(generate_interface_impl_add_reference(generated, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "(void *const self, ptrdiff_t const difference)\n"));
    LPG_TRY(stream_writer_write_string(c_output, "{\n"));

    LPG_TRY(indent(1, c_output));
    LPG_TRY(
        stream_writer_write_string(c_output, "size_t *const counter = (size_t *)((char *)self - sizeof(*counter));\n"));
    LPG_TRY(indent(1, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "*counter = (size_t)((ptrdiff_t)*counter + difference);\n"));
    LPG_TRY(indent(1, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "if (*counter == 0)\n"));
    LPG_TRY(indent(1, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "{\n"));
    {
        memory_writer freed = {NULL, 0, 0};
        stream_writer freed_writer = memory_writer_erase(&freed);
        LPG_TRY(stream_writer_write_string(freed_writer, "(*("));
        LPG_TRY(generate_type(
            impl.self, standard_library, definitions, all_functions, all_interfaces, all_enums, freed_writer));
        LPG_TRY(stream_writer_write_string(freed_writer, " *)self)"));
        LPG_TRY(generate_free(standard_library, unicode_view_create(freed.data, freed.used), impl.self, all_functions,
                              all_structures, all_enums, 2, c_output));
        memory_writer_free(&freed);
    }
    LPG_TRY(indent(2, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "free(counter);\n"));

    LPG_TRY(indent(1, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "}\n"));

    LPG_TRY(stream_writer_write_string(c_output, "}\n"));
    LPG_TRY(stream_writer_write_string(c_output, "static "));
    LPG_TRY(generate_interface_vtable_name(generated.target, c_output));
    LPG_TRY(stream_writer_write_string(c_output, " const "));
    LPG_TRY(generate_interface_impl_name(generated, c_output));
    LPG_TRY(stream_writer_write_string(c_output, " = {"));
    LPG_TRY(generate_interface_impl_add_reference(generated, c_output));
    for (function_id i = 0; i < impl.target.method_count; ++i)
    {
        LPG_TRY(stream_writer_write_string(c_output, ", "));
        LPG_TRY(generate_function_name(impl.target.methods[i].code, c_output));
    }
    LPG_TRY(stream_writer_write_string(c_output, "};\n"));
    return success;
}

static success_indicator generate_struct_member_name(unicode_view const name, stream_writer const c_output)
{
    LPG_TRY(stream_writer_write_string(c_output, "e_"));
    for (size_t i = 0; i < name.length; ++i)
    {
        if (name.begin[i] == '-')
        {
            LPG_TRY(stream_writer_write_string(c_output, "_"));
        }
        else
        {
            LPG_TRY(stream_writer_write_unicode_view(c_output, unicode_view_create(name.begin + i, 1)));
        }
    }
    return success;
}

static success_indicator generate_struct_definition(struct_id const id, structure const generated,
                                                    standard_library_usage *const standard_library,
                                                    type_definitions *const definitions,
                                                    checked_function const *const all_functions,
                                                    interface const *const all_interfaces,
                                                    enumeration const *const all_enums, stream_writer const c_output)
{
    LPG_TRY(stream_writer_write_string(c_output, "struct "));
    LPG_TRY(generate_struct_name(id, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "\n"));
    LPG_TRY(stream_writer_write_string(c_output, "{\n"));
    if (generated.count == 0)
    {
        standard_library->using_unit = true;
        LPG_TRY(indent(1, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "unit dummy;\n"));
    }
    for (struct_member_id i = 0; i < generated.count; ++i)
    {
        structure_member const member = generated.members[i];
        LPG_TRY(indent(1, c_output));
        LPG_TRY(generate_type(
            member.what, standard_library, definitions, all_functions, all_interfaces, all_enums, c_output));
        LPG_TRY(stream_writer_write_string(c_output, " "));
        LPG_TRY(generate_struct_member_name(unicode_view_from_string(member.name), c_output));
        LPG_TRY(stream_writer_write_string(c_output, ";\n"));
    }
    LPG_TRY(stream_writer_write_string(c_output, "};\n"));
    return success;
}

static success_indicator generate_stateful_enum_name(enum_id const enumeration, stream_writer const c_output)
{
    LPG_TRY(stream_writer_write_string(c_output, "enum_"));
    LPG_TRY(stream_writer_write_integer(c_output, integer_create(0, enumeration)));
    return success;
}

static success_indicator
generate_stateful_enum_definition(enum_id const id, standard_library_usage *const standard_library,
                                  type_definitions *const definitions, checked_function const *const all_functions,
                                  interface const *const all_interfaces, enumeration const *const all_enums,
                                  stream_writer const c_output)
{
    enumeration const generated = all_enums[id];
    LPG_TRY(stream_writer_write_string(c_output, "struct "));
    LPG_TRY(generate_stateful_enum_name(id, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "\n"));
    LPG_TRY(stream_writer_write_string(c_output, "{\n"));
    LPG_TRY(indent(1, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "size_t which;\n"));
    LPG_TRY(indent(1, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "union\n"));
    LPG_TRY(indent(1, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "{\n"));
    for (struct_member_id i = 0; i < generated.size; ++i)
    {
        enumeration_element const member = generated.elements[i];
        LPG_TRY(indent(2, c_output));
        LPG_TRY(generate_type(
            member.state, standard_library, definitions, all_functions, all_interfaces, all_enums, c_output));
        LPG_TRY(stream_writer_write_string(c_output, " "));
        LPG_TRY(generate_struct_member_name(unicode_view_from_string(member.name), c_output));
        LPG_TRY(stream_writer_write_string(c_output, ";\n"));
    }
    LPG_TRY(indent(1, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "};\n"));
    LPG_TRY(stream_writer_write_string(c_output, "};\n"));
    return success;
}

static success_indicator generate_type(type const generated, standard_library_usage *const standard_library,
                                       type_definitions *const definitions, checked_function const *const all_functions,
                                       interface const *const all_interfaces, enumeration const *const all_enums,
                                       stream_writer const c_output)
{
    switch (generated.kind)
    {
    case type_kind_interface:
        return generate_interface_reference_name(generated.interface_, c_output);

    case type_kind_lambda:
    {
        function_pointer const *const signature = all_functions[generated.lambda.lambda].signature;
        if (signature->captures.length)
        {
            return generate_type(type_from_tuple_type(signature->captures), standard_library, definitions,
                                 all_functions, all_interfaces, all_enums, c_output);
        }
        return generate_c_function_pointer(type_from_function_pointer(signature), standard_library, definitions,
                                           all_functions, all_interfaces, all_enums, c_output);
    }

    case type_kind_function_pointer:
        if (generated.function_pointer_->captures.length)
        {
            return generate_type(type_from_tuple_type(generated.function_pointer_->captures), standard_library,
                                 definitions, all_functions, all_interfaces, all_enums, c_output);
        }
        return generate_c_function_pointer(
            generated, standard_library, definitions, all_functions, all_interfaces, all_enums, c_output);

    case type_kind_unit:
        standard_library->using_unit = true;
        return stream_writer_write_string(c_output, "unit");

    case type_kind_string_ref:
        standard_library_usage_use_string_ref(standard_library);
        return stream_writer_write_string(c_output, "string_ref");

    case type_kind_enumeration:
        if (has_stateful_element(all_enums[generated.enum_]))
        {
            return generate_stateful_enum_name(generated.enum_, c_output);
        }
        return stream_writer_write_string(c_output, "stateless_enum");

    case type_kind_tuple:
    {
        unicode_string const *const existing_definition = find_type_definition(*definitions, generated);
        if (existing_definition)
        {
            return stream_writer_write_unicode_view(c_output, unicode_view_from_string(*existing_definition));
        }
        memory_writer definition_buffer = {NULL, 0, 0};
        stream_writer definition_writer = memory_writer_erase(&definition_buffer);
        size_t const definition_index = definitions->count;
        ++(definitions->count);
        unicode_string const name = make_type_definition_name(definition_index);
        definitions->elements =
            reallocate_array(definitions->elements, (definitions->count + 1), sizeof(*definitions->elements));
        {
            type_definition *const new_definition = definitions->elements + definition_index;
            new_definition->what = generated;
            new_definition->name = name;
        }
        if (generated.tuple_.length > 0)
        {
            LPG_TRY(stream_writer_write_string(definition_writer, "typedef struct "));
            LPG_TRY(generate_type(
                generated, standard_library, definitions, all_functions, all_interfaces, all_enums, definition_writer));
            LPG_TRY(stream_writer_write_string(definition_writer, "\n"));
            LPG_TRY(stream_writer_write_string(definition_writer, "{\n"));
            for (struct_member_id i = 0; i < generated.tuple_.length; ++i)
            {
                LPG_TRY(indent(1, definition_writer));
                LPG_TRY(generate_type(generated.tuple_.elements[i], standard_library, definitions, all_functions,
                                      all_interfaces, all_enums, definition_writer));
                LPG_TRY(stream_writer_write_string(definition_writer, " "));
                LPG_TRY(generate_tuple_element_name(i, definition_writer));
                LPG_TRY(stream_writer_write_string(definition_writer, ";\n"));
            }
            LPG_TRY(stream_writer_write_string(definition_writer, "} "));
            /*no newline here because clang-format will replace it with a
             * space*/
        }
        else
        {
            standard_library->using_unit = true;
            LPG_TRY(stream_writer_write_string(definition_writer, "typedef unit "));
        }
        LPG_TRY(generate_type(
            generated, standard_library, definitions, all_functions, all_interfaces, all_enums, definition_writer));
        LPG_TRY(stream_writer_write_string(definition_writer, ";\n"));
        type_definition *const new_definition = definitions->elements + definition_index;
        new_definition->definition.data = definition_buffer.data;
        new_definition->definition.length = definition_buffer.used;
        new_definition->order = definitions->next_order++;
        return stream_writer_write_unicode_view(c_output, unicode_view_from_string(name));
    }

    case type_kind_integer_range:
        standard_library->using_stdint = true;
        return stream_writer_write_string(c_output, "uint64_t");

    case type_kind_type:
        standard_library->using_unit = true;
        return stream_writer_write_string(c_output, "unit");

    case type_kind_enum_constructor:
    case type_kind_method_pointer:
        LPG_TO_DO();

    case type_kind_structure:
        return generate_struct_name(generated.structure_, c_output);
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_c_read_access(c_backend_state *state, checked_function const *const current_function,
                                                register_id const from, stream_writer const c_output)
{
    switch (state->registers[from].meaning)
    {
    case register_meaning_nothing:
        LPG_UNREACHABLE();

    case register_meaning_variable:
    case register_meaning_argument:
        return generate_register_name(from, current_function, c_output);

    case register_meaning_global:
    case register_meaning_string_equals:
    case register_meaning_concat:
    case register_meaning_captures:
    case register_meaning_integer_less:
    case register_meaning_side_effect:
    case register_meaning_integer_to_string:
        LPG_TO_DO();

    case register_meaning_and:
        state->standard_library.using_boolean = true;
        return stream_writer_write_string(c_output, "and_impl");

    case register_meaning_or:
        state->standard_library.using_boolean = true;
        return stream_writer_write_string(c_output, "or_impl");

    case register_meaning_not:
        state->standard_library.using_boolean = true;
        return stream_writer_write_string(c_output, "not_impl");

    case register_meaning_assert:
        state->standard_library.using_assert = true;
        return stream_writer_write_string(c_output, "assert_impl");

    case register_meaning_integer_equals:
        state->standard_library.using_integer = true;
        return stream_writer_write_string(c_output, "integer_equals");

    case register_meaning_capture:
        LPG_TRY(stream_writer_write_string(c_output, "captures->"));
        LPG_TRY(generate_tuple_element_name(state->registers[from].capture, c_output));
        return success;

    case register_meaning_unit:
        state->standard_library.using_unit = true;
        return stream_writer_write_string(c_output, "unit_impl");
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_add_reference(unicode_view const value, type const what, size_t const indentation,
                                                checked_function const *const all_functions,
                                                structure const *const all_structures, stream_writer const c_output);

static success_indicator generate_add_reference_to_tuple(unicode_view const value, type const *const elements,
                                                         size_t const element_count, size_t const indentation,
                                                         checked_function const *const all_functions,
                                                         structure const *const all_structures,
                                                         stream_writer const c_output)
{
    for (struct_member_id i = 0; i < element_count; ++i)
    {
        memory_writer name_buffer = {NULL, 0, 0};
        LPG_TRY(stream_writer_write_unicode_view(memory_writer_erase(&name_buffer), value));
        LPG_TRY(stream_writer_write_string(memory_writer_erase(&name_buffer), "."));
        LPG_TRY(generate_tuple_element_name(i, memory_writer_erase(&name_buffer)));
        LPG_TRY(generate_add_reference(
            memory_writer_content(name_buffer), elements[i], indentation, all_functions, all_structures, c_output));
        memory_writer_free(&name_buffer);
    }
    return success;
}

static success_indicator generate_add_reference_to_structure(unicode_view const value, structure const type_of,
                                                             size_t const indentation,
                                                             checked_function const *const all_functions,
                                                             structure const *const all_structures,
                                                             stream_writer const c_output)
{
    for (struct_member_id i = 0; i < type_of.count; ++i)
    {
        memory_writer name_buffer = {NULL, 0, 0};
        LPG_TRY(stream_writer_write_unicode_view(memory_writer_erase(&name_buffer), value));
        LPG_TRY(stream_writer_write_string(memory_writer_erase(&name_buffer), "."));
        LPG_TRY(generate_struct_member_name(
            unicode_view_from_string(type_of.members[i].name), memory_writer_erase(&name_buffer)));
        LPG_TRY(generate_add_reference(memory_writer_content(name_buffer), type_of.members[i].what, indentation,
                                       all_functions, all_structures, c_output));
        memory_writer_free(&name_buffer);
    }
    return success;
}

static success_indicator generate_add_reference(unicode_view const value, type const what, size_t const indentation,
                                                checked_function const *const all_functions,
                                                structure const *const all_structures, stream_writer const c_output)
{
    switch (what.kind)
    {
    case type_kind_string_ref:
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "string_ref_add_reference(&"));
        LPG_TRY(stream_writer_write_unicode_view(c_output, value));
        return stream_writer_write_string(c_output, ");\n");

    case type_kind_enumeration:
    case type_kind_unit:
    case type_kind_integer_range:
    case type_kind_type:
    case type_kind_function_pointer:
        return success;

    case type_kind_tuple:
        return generate_add_reference_to_tuple(
            value, what.tuple_.elements, what.tuple_.length, indentation, all_functions, all_structures, c_output);

    case type_kind_lambda:
    {
        tuple_type const captures = all_functions[what.lambda.lambda].signature->captures;
        return generate_add_reference_to_tuple(
            value, captures.elements, captures.length, indentation, all_functions, all_structures, c_output);
    }

    case type_kind_interface:
        LPG_TRY(stream_writer_write_unicode_view(c_output, value));
        LPG_TRY(stream_writer_write_string(c_output, ".vtable->_add_reference("));
        LPG_TRY(stream_writer_write_unicode_view(c_output, value));
        LPG_TRY(stream_writer_write_string(c_output, ".self, 1);\n"));
        return success;

    case type_kind_structure:
    {
        return generate_add_reference_to_structure(
            value, all_structures[what.structure_], indentation, all_functions, all_structures, c_output);
    }

    case type_kind_method_pointer:
    case type_kind_enum_constructor:
        LPG_TO_DO();
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_add_reference_to_register(
    checked_function const *const current_function, register_id const where, type const what, size_t const indentation,
    checked_function const *const all_functions, structure const *const all_structures, stream_writer const c_output)
{
    memory_writer name_buffer = {NULL, 0, 0};
    LPG_TRY(generate_register_name(where, current_function, memory_writer_erase(&name_buffer)));
    success_indicator const result = generate_add_reference(
        memory_writer_content(name_buffer), what, indentation, all_functions, all_structures, c_output);
    memory_writer_free(&name_buffer);
    return result;
}

static success_indicator generate_add_reference_for_return_value(c_backend_state *state,
                                                                 checked_function const *const current_function,
                                                                 register_id const from, size_t const indentation,
                                                                 checked_function const *const all_functions,
                                                                 structure const *const all_structures,
                                                                 stream_writer const c_output)
{
    switch (state->registers[from].meaning)
    {
    case register_meaning_capture:
    case register_meaning_variable:
    case register_meaning_argument:
        switch (state->registers[from].ownership)
        {
        case register_resource_ownership_owns:
            return success;

        case register_resource_ownership_borrows:
            ASSUME(state->registers[from].type_of.is_set);
            return generate_add_reference_to_register(current_function, from, state->registers[from].type_of.value,
                                                      indentation, all_functions, all_structures, c_output);
        }

    case register_meaning_assert:
    case register_meaning_string_equals:
    case register_meaning_integer_equals:
    case register_meaning_concat:
    case register_meaning_side_effect:
    case register_meaning_and:
    case register_meaning_not:
    case register_meaning_or:
    case register_meaning_integer_less:
    case register_meaning_integer_to_string:
    case register_meaning_unit:
        return success;

    case register_meaning_nothing:
    case register_meaning_captures:
    case register_meaning_global:
        LPG_UNREACHABLE();
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_sequence(c_backend_state *state, checked_function const *const all_functions,
                                           interface const *const all_interfaces,
                                           checked_function const *const current_function,
                                           register_id const current_function_result,
                                           instruction_sequence const sequence, size_t const indentation,
                                           stream_writer const c_output);

static type find_boolean(void)
{
    return type_from_enumeration(0);
}

static success_indicator generate_tuple_initializer(c_backend_state *state,
                                                    checked_function const *const current_function,
                                                    size_t const element_count, register_id *const elements,
                                                    stream_writer const c_output)
{
    LPG_TRY(stream_writer_write_string(c_output, "{"));
    if (element_count > 0)
    {
        for (size_t i = 0; i < element_count; ++i)
        {
            if (i > 0)
            {
                LPG_TRY(stream_writer_write_string(c_output, ", "));
            }
            LPG_TRY(generate_c_read_access(state, current_function, elements[i], c_output));
        }
    }
    else
    {
        /*for dummy in struct unit*/
        LPG_TRY(stream_writer_write_string(c_output, "0"));
    }
    LPG_TRY(stream_writer_write_string(c_output, "}"));
    return success;
}

static success_indicator generate_tuple_variable(c_backend_state *state, checked_function const *const current_function,
                                                 tuple_type const tuple, register_id *const elements,
                                                 register_id const result, size_t const indentation,
                                                 stream_writer const c_output)
{
    set_register_variable(state, result, register_resource_ownership_owns, type_from_tuple_type(tuple));
    for (size_t i = 0; i < tuple.length; ++i)
    {
        LPG_TRY(generate_add_reference_to_register(current_function, elements[i], tuple.elements[i], indentation,
                                                   state->all_functions, state->all_structs, c_output));
    }
    LPG_TRY(indent(indentation, c_output));
    LPG_TRY(generate_type(type_from_tuple_type(tuple), &state->standard_library, state->definitions,
                          state->all_functions, state->all_interfaces, state->all_enums, c_output));
    LPG_TRY(stream_writer_write_string(c_output, " const "));
    LPG_TRY(generate_register_name(result, current_function, c_output));
    LPG_TRY(stream_writer_write_string(c_output, " = "));
    LPG_TRY(generate_tuple_initializer(state, current_function, tuple.length, elements, c_output));
    LPG_TRY(stream_writer_write_string(c_output, ";\n"));
    return success;
}

static success_indicator generate_instantiate_struct(c_backend_state *state,
                                                     checked_function const *const current_function,
                                                     instantiate_struct_instruction const generated,
                                                     size_t const indentation, stream_writer const c_output)
{
    set_register_variable(
        state, generated.into, register_resource_ownership_owns, type_from_struct(generated.instantiated));
    for (size_t i = 0; i < generated.argument_count; ++i)
    {
        register_id const argument = generated.arguments[i];
        ASSUME(state->registers[argument].type_of.is_set);
        LPG_TRY(generate_add_reference_to_register(current_function, argument, state->registers[argument].type_of.value,
                                                   indentation, state->all_functions, state->all_structs, c_output));
    }
    LPG_TRY(indent(indentation, c_output));
    LPG_TRY(generate_type(type_from_struct(generated.instantiated), &state->standard_library, state->definitions,
                          state->all_functions, state->all_interfaces, state->all_enums, c_output));
    LPG_TRY(stream_writer_write_string(c_output, " const "));
    LPG_TRY(generate_register_name(generated.into, current_function, c_output));
    LPG_TRY(stream_writer_write_string(c_output, " = "));
    LPG_TRY(
        generate_tuple_initializer(state, current_function, generated.argument_count, generated.arguments, c_output));
    LPG_TRY(stream_writer_write_string(c_output, ";\n"));
    return success;
}

static success_indicator generate_erase_type(c_backend_state *state, register_id const destination,
                                             implementation_ref const impl, unicode_view const self,
                                             bool const add_reference_to_self,
                                             checked_function const *const all_functions,
                                             interface const *const all_interfaces,
                                             checked_function const *const current_function, size_t const indentation,
                                             stream_writer const c_output)
{
    LPG_TRY(generate_interface_reference_name(impl.target, c_output));
    LPG_TRY(stream_writer_write_string(c_output, " "));
    LPG_TRY(generate_register_name(destination, current_function, c_output));
    LPG_TRY(stream_writer_write_string(c_output, " = {&"));
    LPG_TRY(generate_interface_impl_name(impl, c_output));
    LPG_TRY(stream_writer_write_string(c_output, ", malloc(sizeof(size_t) + sizeof("));
    LPG_TRY(stream_writer_write_unicode_view(c_output, self));
    LPG_TRY(stream_writer_write_string(c_output, "))};\n"));

    LPG_TRY(indent(indentation, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "*(size_t *)"));
    LPG_TRY(generate_register_name(destination, current_function, c_output));
    LPG_TRY(stream_writer_write_string(c_output, ".self = 1;\n"));

    LPG_TRY(indent(indentation, c_output));
    LPG_TRY(generate_register_name(destination, current_function, c_output));
    LPG_TRY(stream_writer_write_string(c_output, ".self = (char *)"));
    LPG_TRY(generate_register_name(destination, current_function, c_output));
    LPG_TRY(stream_writer_write_string(c_output, ".self + sizeof(size_t);\n"));

    LPG_TRY(indent(indentation, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "*("));
    type const self_type = all_interfaces[impl.target].implementations[impl.implementation_index].self;
    LPG_TRY(generate_type(self_type, &state->standard_library, state->definitions, all_functions, all_interfaces,
                          state->all_enums, c_output));
    LPG_TRY(stream_writer_write_string(c_output, " *)"));
    LPG_TRY(generate_register_name(destination, current_function, c_output));
    LPG_TRY(stream_writer_write_string(c_output, ".self = "));
    LPG_TRY(stream_writer_write_unicode_view(c_output, self));
    LPG_TRY(stream_writer_write_string(c_output, ";\n"));

    if (add_reference_to_self)
    {
        LPG_TRY(
            generate_add_reference(self, self_type, indentation, state->all_functions, state->all_structs, c_output));
    }
    return success;
}

static success_indicator generate_value(value const generated, type const type_of, c_backend_state *state,
                                        stream_writer const c_output)
{
    switch (generated.kind)
    {
    case value_kind_integer:
    {
        if (integer_less(generated.integer_, integer_create(1, 0)))
        {
            char buffer[40];
            char *formatted = integer_format(generated.integer_, lower_case_digits, 10, buffer, sizeof(buffer));
            LPG_TRY(stream_writer_write_bytes(c_output, formatted, (size_t)((buffer + sizeof(buffer)) - formatted)));
            return success;
        }
        LPG_TO_DO();
    }

    case value_kind_string:
        state->standard_library.using_string_ref = true;
        LPG_TRY(stream_writer_write_string(c_output, "string_literal("));
        LPG_TRY(encode_string_literal(generated.string_ref, c_output));
        LPG_TRY(stream_writer_write_string(c_output, ", "));
        {
            char buffer[40];
            char *formatted = integer_format(
                integer_create(0, generated.string_ref.length), lower_case_digits, 10, buffer, sizeof(buffer));
            LPG_TRY(stream_writer_write_bytes(c_output, formatted, (size_t)((buffer + sizeof(buffer)) - formatted)));
        }
        LPG_TRY(stream_writer_write_string(c_output, ")"));
        return success;

    case value_kind_function_pointer:
    {
        if (generated.function_pointer.external)
        {
            LPG_TO_DO();
        }
        if (generated.function_pointer.capture_count > 0)
        {
            LPG_TRY(generate_value(
                value_from_tuple(
                    value_tuple_create(generated.function_pointer.captures, generated.function_pointer.capture_count)),
                type_from_tuple_type(state->all_functions[type_of.lambda.lambda].signature->captures), state,
                c_output));
        }
        else
        {
            LPG_TRY(generate_function_name(generated.function_pointer.code, c_output));
        }
        return success;
    }

    case value_kind_tuple:
        ASSUME(type_of.kind == type_kind_tuple);
        ASSUME(generated.tuple_.element_count == type_of.tuple_.length);
        LPG_TRY(stream_writer_write_string(c_output, "{"));
        for (size_t i = 0; i < generated.tuple_.element_count; ++i)
        {
            if (i > 0)
            {
                LPG_TRY(stream_writer_write_string(c_output, ", "));
            }
            LPG_TRY(generate_value(generated.tuple_.elements[i], type_of.tuple_.elements[i], state, c_output));
        }
        LPG_TRY(stream_writer_write_string(c_output, "}"));
        return success;

    case value_kind_type_erased:
    case value_kind_flat_object:
    case value_kind_pattern:
        LPG_TO_DO();

    case value_kind_type:
        state->standard_library.using_unit = true;
        LPG_TRY(stream_writer_write_string(c_output, "unit_impl"));
        return success;

    case value_kind_enum_constructor:
        LPG_TRY(stream_writer_write_string(c_output, "/*enum constructor*/"));
        return success;

    case value_kind_enum_element:
    {
        ASSUME(type_of.kind == type_kind_enumeration);
        enumeration const enum_ = state->all_enums[type_of.enum_];
        if (has_stateful_element(enum_))
        {
            LPG_TRY(stream_writer_write_string(c_output, "{"));
            LPG_TRY(stream_writer_write_integer(c_output, integer_create(0, generated.enum_element.which)));
            LPG_TRY(stream_writer_write_string(c_output, ", {."));
            LPG_TRY(generate_struct_member_name(
                unicode_view_from_string(enum_.elements[generated.enum_element.which].name), c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = "));
            LPG_TRY(generate_value(value_or_unit(generated.enum_element.state),
                                   enum_.elements[generated.enum_element.which].state, state, c_output));
            LPG_TRY(stream_writer_write_string(c_output, "}}"));
            return success;
        }
        char buffer[64];
        char const *const formatted = integer_format(
            integer_create(0, generated.enum_element.which), lower_case_digits, 10, buffer, sizeof(buffer));
        LPG_TRY(stream_writer_write_bytes(c_output, formatted, (size_t)((buffer + sizeof(buffer)) - formatted)));
        return success;
    }

    case value_kind_unit:
        state->standard_library.using_unit = true;
        LPG_TRY(stream_writer_write_string(c_output, "unit_impl"));
        return success;
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_free_register(c_backend_state *state, register_id const which,
                                                checked_function const *const current_function,
                                                size_t const indentation, stream_writer const c_output)
{
    switch (state->registers[which].ownership)
    {
    case register_resource_ownership_owns:
    {
        ASSUME(state->registers[which].type_of.is_set);
        memory_writer name_buffer = {NULL, 0, 0};
        LPG_TRY(generate_register_name(which, current_function, memory_writer_erase(&name_buffer)));
        success_indicator const result = generate_free(&state->standard_library, memory_writer_content(name_buffer),
                                                       state->registers[which].type_of.value, state->all_functions,
                                                       state->all_structs, state->all_enums, indentation, c_output);
        memory_writer_free(&name_buffer);
        return result;
    }

    case register_resource_ownership_borrows:
        return success;
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_free_registers(c_backend_state *state, size_t const previously_active_registers,
                                                 size_t const indentation,
                                                 checked_function const *const current_function,
                                                 register_id const current_function_result,
                                                 stream_writer const c_output)
{
    for (size_t i = previously_active_registers; i < state->active_register_count; ++i)
    {
        register_id const which = state->active_registers[i];
        if (which == current_function_result)
        {
            continue;
        }
        LPG_TRY(generate_free_register(state, which, current_function, indentation, c_output));
    }
    state->active_register_count = previously_active_registers;
    return success;
}

static success_indicator generate_instruction(c_backend_state *state, checked_function const *const all_functions,
                                              interface const *const all_interfaces, enumeration const *const all_enums,
                                              checked_function const *const current_function,
                                              register_id const current_function_result, instruction const input,
                                              size_t const indentation, stream_writer const c_output)
{
    switch (input.type)
    {
    case instruction_erase_type:
    {
        set_register_variable(state, input.erase_type.into, register_resource_ownership_owns,
                              type_from_interface(input.erase_type.impl.target));
        memory_writer original_self = {NULL, 0, 0};
        LPG_TRY(generate_c_read_access(
            state, current_function, input.erase_type.self, memory_writer_erase(&original_self)));
        bool add_reference_to_self = false;
        switch (state->registers[input.erase_type.self].meaning)
        {
        case register_meaning_argument:
        case register_meaning_variable:
        case register_meaning_capture:
            add_reference_to_self = true;
            break;

        case register_meaning_nothing:
        case register_meaning_global:
        case register_meaning_assert:
        case register_meaning_string_equals:
        case register_meaning_integer_equals:
        case register_meaning_integer_less:
        case register_meaning_integer_to_string:
        case register_meaning_or:
        case register_meaning_and:
        case register_meaning_not:
        case register_meaning_concat:
        case register_meaning_captures:
        case register_meaning_side_effect:
        case register_meaning_unit:
            break;
        }
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(generate_erase_type(state, input.erase_type.into, input.erase_type.impl,
                                    unicode_view_create(original_self.data, original_self.used), add_reference_to_self,
                                    all_functions, all_interfaces, current_function, indentation, c_output));
        memory_writer_free(&original_self);
        return success;
    }

    case instruction_get_method:
    {
        set_register_variable(
            state, input.get_method.into, register_resource_ownership_borrows,
            type_from_method_pointer(method_pointer_type_create(input.get_method.interface_, input.get_method.method)));
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(generate_interface_reference_name(input.get_method.interface_, c_output));
        LPG_TRY(stream_writer_write_string(c_output, " "));
        LPG_TRY(generate_register_name(input.get_method.into, current_function, c_output));
        LPG_TRY(stream_writer_write_string(c_output, " = "));
        LPG_TRY(generate_c_read_access(state, current_function, input.get_method.from, c_output));
        LPG_TRY(stream_writer_write_string(c_output, ";\n"));
        return success;
    }

    case instruction_return:
        LPG_TO_DO();

    case instruction_call:
        LPG_TRY(indent(indentation, c_output));
        switch (state->registers[input.call.callee].meaning)
        {
        case register_meaning_nothing:
        case register_meaning_global:
        case register_meaning_unit:
            LPG_UNREACHABLE();

        case register_meaning_side_effect:
            state->standard_library.using_unit = true;
            set_register_variable(state, input.call.result, register_resource_ownership_owns, type_from_unit());
            LPG_TRY(stream_writer_write_string(c_output, "/*side effect*/\n"));
            LPG_TRY(indent(indentation, c_output));
            LPG_TRY(stream_writer_write_string(c_output, "unit const "));
            LPG_TRY(generate_register_name(input.call.result, current_function, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = unit_impl;\n"));
            return success;

        case register_meaning_assert:
            state->standard_library.using_assert = true;
            set_register_variable(state, input.call.result, register_resource_ownership_owns, type_from_unit());
            LPG_TRY(stream_writer_write_string(c_output, "unit const "));
            LPG_TRY(generate_register_name(input.call.result, current_function, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = assert_impl("));
            ASSERT(input.call.argument_count == 1);
            LPG_TRY(generate_c_read_access(state, current_function, input.call.arguments[0], c_output));
            LPG_TRY(stream_writer_write_string(c_output, ");\n"));
            return success;

        case register_meaning_string_equals:
            standard_library_usage_use_string_ref(&state->standard_library);
            set_register_variable(state, input.call.result, register_resource_ownership_owns, find_boolean());
            LPG_TRY(stream_writer_write_string(c_output, "bool const "));
            LPG_TRY(generate_register_name(input.call.result, current_function, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = string_ref_equals("));
            ASSERT(input.call.argument_count == 2);
            LPG_TRY(generate_c_read_access(state, current_function, input.call.arguments[0], c_output));
            LPG_TRY(stream_writer_write_string(c_output, ", "));
            LPG_TRY(generate_c_read_access(state, current_function, input.call.arguments[1], c_output));
            LPG_TRY(stream_writer_write_string(c_output, ");\n"));
            return success;

        case register_meaning_integer_equals:
            state->standard_library.using_integer = true;
            set_register_variable(state, input.call.result, register_resource_ownership_owns, find_boolean());
            LPG_TRY(stream_writer_write_string(c_output, "bool const "));
            LPG_TRY(generate_register_name(input.call.result, current_function, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = integer_equals("));
            ASSERT(input.call.argument_count == 2);
            LPG_TRY(generate_c_read_access(state, current_function, input.call.arguments[0], c_output));
            LPG_TRY(stream_writer_write_string(c_output, ", "));
            LPG_TRY(generate_c_read_access(state, current_function, input.call.arguments[1], c_output));
            LPG_TRY(stream_writer_write_string(c_output, ");\n"));
            return success;

        case register_meaning_integer_less:
            state->standard_library.using_integer = true;
            set_register_variable(state, input.call.result, register_resource_ownership_owns, find_boolean());
            LPG_TRY(stream_writer_write_string(c_output, "bool const "));
            LPG_TRY(generate_register_name(input.call.result, current_function, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = integer_less("));
            ASSERT(input.call.argument_count == 2);
            LPG_TRY(generate_c_read_access(state, current_function, input.call.arguments[0], c_output));
            LPG_TRY(stream_writer_write_string(c_output, ", "));
            LPG_TRY(generate_c_read_access(state, current_function, input.call.arguments[1], c_output));
            LPG_TRY(stream_writer_write_string(c_output, ");\n"));
            return success;

        case register_meaning_integer_to_string:
            state->standard_library.using_integer = true;
            standard_library_usage_use_string_ref(&state->standard_library);
            set_register_variable(state, input.call.result, register_resource_ownership_owns, type_from_string_ref());
            LPG_TRY(stream_writer_write_string(c_output, "string_ref const "));
            LPG_TRY(generate_register_name(input.call.result, current_function, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = integer_to_string("));
            ASSERT(input.call.argument_count == 1);
            LPG_TRY(generate_c_read_access(state, current_function, input.call.arguments[0], c_output));
            LPG_TRY(stream_writer_write_string(c_output, ");\n"));
            return success;

        case register_meaning_concat:
            standard_library_usage_use_string_ref(&state->standard_library);
            set_register_variable(state, input.call.result, register_resource_ownership_owns, type_from_string_ref());
            LPG_TRY(stream_writer_write_string(c_output, "string_ref const "));
            LPG_TRY(generate_register_name(input.call.result, current_function, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = string_ref_concat("));
            ASSERT(input.call.argument_count == 2);
            LPG_TRY(generate_c_read_access(state, current_function, input.call.arguments[0], c_output));
            LPG_TRY(stream_writer_write_string(c_output, ", "));
            LPG_TRY(generate_c_read_access(state, current_function, input.call.arguments[1], c_output));
            LPG_TRY(stream_writer_write_string(c_output, ");\n"));
            return success;

        case register_meaning_capture:
        case register_meaning_variable:
        {
            type result_type = type_from_unit();
            ASSUME(state->registers[input.call.callee].type_of.is_set);
            type const callee_type = state->registers[input.call.callee].type_of.value;
            switch (callee_type.kind)
            {
            case type_kind_method_pointer:
            {
                method_description const called_method = all_interfaces[callee_type.method_pointer.interface_]
                                                             .methods[callee_type.method_pointer.method_index];
                set_register_function_variable(
                    state, input.call.result, register_resource_ownership_owns, called_method.result);
                LPG_TRY(generate_type(called_method.result, &state->standard_library, state->definitions,
                                      state->all_functions, state->all_interfaces, state->all_enums, c_output));
                LPG_TRY(stream_writer_write_string(c_output, " const "));
                LPG_TRY(generate_register_name(input.call.result, current_function, c_output));
                LPG_TRY(stream_writer_write_string(c_output, " = "));
                LPG_TRY(generate_c_read_access(state, current_function, input.call.callee, c_output));
                LPG_TRY(stream_writer_write_string(c_output, ".vtable->"));
                LPG_TRY(escape_identifier(unicode_view_from_string(called_method.name), c_output));
                LPG_TRY(stream_writer_write_string(c_output, "("));
                LPG_TRY(generate_c_read_access(state, current_function, input.call.callee, c_output));
                LPG_TRY(stream_writer_write_string(c_output, ".self"));
                for (size_t i = 0; i < input.call.argument_count; ++i)
                {
                    LPG_TRY(stream_writer_write_string(c_output, ", "));
                    LPG_TRY(generate_c_read_access(state, current_function, input.call.arguments[i], c_output));
                }
                LPG_TRY(stream_writer_write_string(c_output, ");\n"));
                return success;
            }

            case type_kind_function_pointer:
                result_type = callee_type.function_pointer_->result;
                break;

            case type_kind_interface:
            case type_kind_structure:
            case type_kind_unit:
            case type_kind_string_ref:
            case type_kind_enumeration:
            case type_kind_tuple:
            case type_kind_type:
            case type_kind_integer_range:
                LPG_UNREACHABLE();

            case type_kind_enum_constructor:
                LPG_TO_DO();

            case type_kind_lambda:
            {
                ASSUME(state->registers[input.call.callee].type_of.is_set);
                function_pointer const callee_signature =
                    *all_functions[state->registers[input.call.callee].type_of.value.lambda.lambda].signature;
                result_type = callee_signature.result;
                set_register_function_variable(state, input.call.result, register_resource_ownership_owns, result_type);
                LPG_TRY(generate_type(result_type, &state->standard_library, state->definitions, state->all_functions,
                                      state->all_interfaces, state->all_enums, c_output));
                LPG_TRY(stream_writer_write_string(c_output, " const "));
                LPG_TRY(generate_register_name(input.call.result, current_function, c_output));
                LPG_TRY(stream_writer_write_string(c_output, " = "));
                LPG_TRY(
                    generate_function_name(state->registers[input.call.callee].type_of.value.lambda.lambda, c_output));
                LPG_TRY(stream_writer_write_string(c_output, "("));
                bool comma = false;
                if (callee_signature.captures.length > 0)
                {
                    LPG_TRY(stream_writer_write_string(c_output, "&"));
                    LPG_TRY(generate_c_read_access(state, current_function, input.call.callee, c_output));
                    comma = true;
                }
                for (size_t i = 0; i < input.call.argument_count; ++i)
                {
                    if (comma)
                    {
                        LPG_TRY(stream_writer_write_string(c_output, ", "));
                    }
                    else
                    {
                        comma = true;
                    }
                    LPG_TRY(generate_c_read_access(state, current_function, input.call.arguments[i], c_output));
                }
                LPG_TRY(stream_writer_write_string(c_output, ");\n"));
                return success;
            }
            }
            set_register_function_variable(state, input.call.result, register_resource_ownership_owns, result_type);
            LPG_TRY(generate_type(result_type, &state->standard_library, state->definitions, state->all_functions,
                                  state->all_interfaces, state->all_enums, c_output));
            break;
        }

        case register_meaning_and:
            ASSUME(input.call.argument_count == 2);
            set_register_variable(state, input.call.result, register_resource_ownership_owns, find_boolean());
            LPG_TRY(stream_writer_write_string(c_output, "size_t const "));
            LPG_TRY(generate_register_name(input.call.result, current_function, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = ("));
            LPG_TRY(generate_c_read_access(state, current_function, input.call.arguments[0], c_output));
            LPG_TRY(stream_writer_write_string(c_output, " & "));
            LPG_TRY(generate_c_read_access(state, current_function, input.call.arguments[1], c_output));
            LPG_TRY(stream_writer_write_string(c_output, ");\n"));
            return success;

        case register_meaning_or:
            ASSUME(input.call.argument_count == 2);
            set_register_variable(state, input.call.result, register_resource_ownership_owns, find_boolean());
            LPG_TRY(stream_writer_write_string(c_output, "size_t const "));
            LPG_TRY(generate_register_name(input.call.result, current_function, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = ("));
            LPG_TRY(generate_c_read_access(state, current_function, input.call.arguments[0], c_output));
            LPG_TRY(stream_writer_write_string(c_output, " | "));
            LPG_TRY(generate_c_read_access(state, current_function, input.call.arguments[1], c_output));
            LPG_TRY(stream_writer_write_string(c_output, ");\n"));
            return success;

        case register_meaning_not:
            ASSUME(input.call.argument_count == 1);
            set_register_variable(state, input.call.result, register_resource_ownership_owns, find_boolean());
            LPG_TRY(stream_writer_write_string(c_output, "size_t const "));
            LPG_TRY(generate_register_name(input.call.result, current_function, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = !"));
            LPG_TRY(generate_c_read_access(state, current_function, input.call.arguments[0], c_output));
            LPG_TRY(stream_writer_write_string(c_output, ";\n"));
            return success;

        case register_meaning_argument:
            LPG_TO_DO();

        case register_meaning_captures:
            LPG_UNREACHABLE();
        }
        LPG_TRY(stream_writer_write_string(c_output, " const "));
        LPG_TRY(generate_register_name(input.call.result, current_function, c_output));
        LPG_TRY(stream_writer_write_string(c_output, " = "));
        ASSUME(state->registers[input.call.callee].type_of.is_set);
        ASSUME(state->registers[input.call.callee].type_of.value.kind == type_kind_function_pointer);
        LPG_TRY(generate_c_read_access(state, current_function, input.call.callee, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "("));
        for (size_t i = 0; i < input.call.argument_count; ++i)
        {
            if (i > 0)
            {
                LPG_TRY(stream_writer_write_string(c_output, ", "));
            }
            LPG_TRY(generate_c_read_access(state, current_function, input.call.arguments[i], c_output));
        }
        LPG_TRY(stream_writer_write_string(c_output, ");\n"));
        return success;

    case instruction_loop:
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "for (;;)\n"));
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "{\n"));
        LPG_TRY(generate_sequence(state, all_functions, all_interfaces, current_function, current_function_result,
                                  input.loop, indentation + 1, c_output));
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "}\n"));
        return success;

    case instruction_global:
        set_register_meaning(
            state, input.global_into, optional_type_create_set(type_from_struct(0)), register_meaning_global);
        return success;

    case instruction_read_struct:
        switch (state->registers[input.read_struct.from_object].meaning)
        {
        case register_meaning_nothing:
        case register_meaning_and:
        case register_meaning_not:
        case register_meaning_or:
        case register_meaning_unit:
            LPG_UNREACHABLE();

        case register_meaning_capture:
            LPG_TO_DO();

        case register_meaning_global:
            switch (input.read_struct.member)
            {
            case 4:
                set_register_meaning(
                    state, input.read_struct.into, optional_type_create_empty(), register_meaning_assert);
                return success;

            case 5:
                set_register_meaning(state, input.read_struct.into, optional_type_create_empty(), register_meaning_and);
                return success;

            case 6:
                set_register_meaning(state, input.read_struct.into, optional_type_create_empty(), register_meaning_or);
                return success;

            case 7:
                set_register_meaning(state, input.read_struct.into, optional_type_create_empty(), register_meaning_not);
                return success;

            case 8:
                set_register_meaning(
                    state, input.read_struct.into, optional_type_create_empty(), register_meaning_concat);
                return success;

            case 9:
                set_register_meaning(
                    state, input.read_struct.into, optional_type_create_empty(), register_meaning_string_equals);
                return success;

            case 12:
                set_register_meaning(
                    state, input.read_struct.into, optional_type_create_empty(), register_meaning_integer_equals);
                return success;

            case 13:
                set_register_meaning(
                    state, input.read_struct.into, optional_type_create_empty(), register_meaning_unit);
                return success;

            case 14:
                state->standard_library.using_unit = true;
                set_register_variable(
                    state, input.read_struct.into, register_resource_ownership_borrows, type_from_unit());
                LPG_TRY(indent(indentation, c_output));
                LPG_TRY(stream_writer_write_string(c_output, "unit const "));
                LPG_TRY(generate_register_name(input.read_struct.into, current_function, c_output));
                LPG_TRY(stream_writer_write_string(c_output, " = unit_impl;\n"));
                return success;

            case 16:
                set_register_meaning(
                    state, input.read_struct.into, optional_type_create_empty(), register_meaning_integer_less);
                return success;

            case 17:
                set_register_meaning(
                    state, input.read_struct.into, optional_type_create_empty(), register_meaning_integer_to_string);
                return success;

            case 18:
                set_register_meaning(
                    state, input.read_struct.into, optional_type_create_empty(), register_meaning_side_effect);
                return success;

            default:
                printf("%u\n", input.read_struct.member);
                LPG_TO_DO();
            }

        case register_meaning_variable:
        case register_meaning_argument:
        {
            ASSUME(state->registers[input.read_struct.from_object].type_of.is_set);
            type const object_type = state->registers[input.read_struct.from_object].type_of.value;
            switch (object_type.kind)
            {
            case type_kind_structure:
            {
                structure const struct_ = state->all_structs[object_type.structure_];
                structure_member const member = struct_.members[input.read_struct.member];
                set_register_variable(state, input.read_struct.into, register_resource_ownership_borrows, member.what);
                LPG_TRY(indent(indentation, c_output));
                LPG_TRY(generate_type(member.what, &state->standard_library, state->definitions, state->all_functions,
                                      state->all_interfaces, state->all_enums, c_output));
                LPG_TRY(stream_writer_write_string(c_output, " const "));
                LPG_TRY(generate_register_name(input.read_struct.into, current_function, c_output));
                LPG_TRY(stream_writer_write_string(c_output, " = "));
                LPG_TRY(generate_register_name(input.read_struct.from_object, current_function, c_output));
                LPG_TRY(stream_writer_write_string(c_output, "."));
                LPG_TRY(generate_struct_member_name(unicode_view_from_string(member.name), c_output));
                LPG_TRY(stream_writer_write_string(c_output, ";\n"));
                return success;
            }

            case type_kind_tuple:
            {
                type const element_type = object_type.tuple_.elements[input.read_struct.member];
                set_register_variable(state, input.read_struct.into, register_resource_ownership_borrows, element_type);
                LPG_TRY(indent(indentation, c_output));
                LPG_TRY(generate_type(element_type, &state->standard_library, state->definitions, state->all_functions,
                                      state->all_interfaces, state->all_enums, c_output));
                LPG_TRY(stream_writer_write_string(c_output, " const "));
                LPG_TRY(generate_register_name(input.read_struct.into, current_function, c_output));
                LPG_TRY(stream_writer_write_string(c_output, " = "));
                LPG_TRY(generate_register_name(input.read_struct.from_object, current_function, c_output));
                LPG_TRY(stream_writer_write_string(c_output, "."));
                LPG_TRY(generate_tuple_element_name(input.read_struct.member, c_output));
                LPG_TRY(stream_writer_write_string(c_output, ";\n"));
                return success;
            }

            case type_kind_method_pointer:
            case type_kind_interface:
            case type_kind_lambda:
            case type_kind_function_pointer:
            case type_kind_unit:
            case type_kind_string_ref:
            case type_kind_enumeration:
            case type_kind_type:
            case type_kind_integer_range:
            case type_kind_enum_constructor:
                LPG_UNREACHABLE();
            }
            LPG_UNREACHABLE();
        }

        case register_meaning_assert:
        case register_meaning_string_equals:
        case register_meaning_integer_equals:
        case register_meaning_integer_less:
        case register_meaning_integer_to_string:
        case register_meaning_concat:
        case register_meaning_side_effect:
            LPG_UNREACHABLE();

        case register_meaning_captures:
        {
            optional_type const capture_tuple = state->registers[input.read_struct.from_object].type_of;
            ASSUME(capture_tuple.is_set);
            ASSUME(capture_tuple.value.kind == type_kind_tuple);
            set_register_to_capture(state, input.read_struct.into, input.read_struct.member,
                                    capture_tuple.value.tuple_.elements[input.read_struct.member]);
            return success;
        }
        }

    case instruction_break:
        state->standard_library.using_unit = true;
        set_register_variable(state, input.break_into, register_resource_ownership_borrows, type_from_unit());
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "unit const "));
        LPG_TRY(generate_register_name(input.break_into, current_function, c_output));
        LPG_TRY(stream_writer_write_string(c_output, " = unit_impl;\n"));
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "break;\n"));
        return success;

    case instruction_literal:
        LPG_TRY(indent(indentation, c_output));
        ASSERT(state->registers[input.literal.into].meaning == register_meaning_nothing);
        switch (input.literal.value_.kind)
        {
        case value_kind_integer:
            set_register_variable(
                state, input.literal.into, register_resource_ownership_borrows, input.literal.type_of);
            LPG_TRY(stream_writer_write_string(c_output, "uint64_t const "));
            LPG_TRY(generate_register_name(input.literal.into, current_function, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = "));
            LPG_TRY(generate_value(input.literal.value_, input.literal.type_of, state, c_output));
            LPG_TRY(stream_writer_write_string(c_output, ";\n"));
            return success;

        case value_kind_string:
            set_register_variable(
                state, input.literal.into, register_resource_ownership_borrows, input.literal.type_of);
            state->standard_library.using_string_ref = true;
            LPG_TRY(stream_writer_write_string(c_output, "string_ref const "));
            LPG_TRY(generate_register_name(input.literal.into, current_function, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = "));
            LPG_TRY(generate_value(input.literal.value_, input.literal.type_of, state, c_output));
            LPG_TRY(stream_writer_write_string(c_output, ";\n"));
            return success;

        case value_kind_function_pointer:
            set_register_variable(
                state, input.literal.into, register_resource_ownership_borrows, input.literal.type_of);
            if (input.literal.value_.function_pointer.external)
            {
                LPG_TO_DO();
            }
            if (input.literal.value_.function_pointer.capture_count > 0)
            {
                tuple_type const captures =
                    all_functions[input.literal.value_.function_pointer.code].signature->captures;
                LPG_TRY(generate_type(type_from_tuple_type(captures), &state->standard_library, state->definitions,
                                      state->all_functions, state->all_interfaces, state->all_enums, c_output));
                LPG_TRY(stream_writer_write_string(c_output, " const "));
                LPG_TRY(generate_register_name(input.literal.into, current_function, c_output));
                LPG_TRY(stream_writer_write_string(c_output, " = "));
                LPG_TRY(generate_value(
                    value_from_tuple(value_tuple_create(input.literal.value_.function_pointer.captures,
                                                        input.literal.value_.function_pointer.capture_count)),
                    type_from_tuple_type(captures), state, c_output));
                LPG_TRY(stream_writer_write_string(c_output, ";\n"));
            }
            else
            {
                LPG_TRY(generate_c_function_pointer(
                    type_from_function_pointer(
                        state->all_functions[input.literal.value_.function_pointer.code].signature),
                    &state->standard_library, state->definitions, state->all_functions, state->all_interfaces,
                    state->all_enums, c_output));
                LPG_TRY(stream_writer_write_string(c_output, " const "));
                LPG_TRY(generate_register_name(input.literal.into, current_function, c_output));
                LPG_TRY(stream_writer_write_string(c_output, " = "));
                LPG_TRY(generate_value(input.literal.value_, input.literal.type_of, state, c_output));
                LPG_TRY(stream_writer_write_string(c_output, ";\n"));
            }
            return success;

        case value_kind_tuple:
            set_register_variable(
                state, input.literal.into, register_resource_ownership_borrows, input.literal.type_of);
            ASSUME(input.literal.type_of.kind == type_kind_tuple);
            ASSUME(input.literal.value_.tuple_.element_count == input.literal.type_of.tuple_.length);
            LPG_TRY(generate_type(input.literal.type_of, &state->standard_library, state->definitions,
                                  state->all_functions, state->all_interfaces, state->all_enums, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " const "));
            LPG_TRY(generate_register_name(input.literal.into, current_function, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = "));
            LPG_TRY(generate_value(input.literal.value_, input.literal.type_of, state, c_output));
            LPG_TRY(stream_writer_write_string(c_output, ";\n"));
            return success;

        case value_kind_type_erased:
        {
            set_register_variable(state, input.literal.into, register_resource_ownership_owns, input.literal.type_of);
            ASSUME(input.literal.type_of.kind == type_kind_interface);
            memory_writer self = {NULL, 0, 0};
            LPG_TRY(generate_value(
                *input.literal.value_.type_erased.self, input.literal.type_of, state, memory_writer_erase(&self)));
            LPG_TRY(generate_erase_type(state, input.literal.into, input.literal.value_.type_erased.impl,
                                        unicode_view_create(self.data, self.used), false, state->all_functions,
                                        state->all_interfaces, current_function, indentation, c_output));
            memory_writer_free(&self);
            return success;
        }

        case value_kind_flat_object:
        case value_kind_pattern:
            LPG_TO_DO();

        case value_kind_type:
            set_register_variable(
                state, input.literal.into, register_resource_ownership_borrows, input.literal.type_of);
            state->standard_library.using_unit = true;
            LPG_TRY(stream_writer_write_string(c_output, "unit const "));
            LPG_TRY(generate_register_name(input.literal.into, current_function, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = "));
            LPG_TRY(generate_value(input.literal.value_, input.literal.type_of, state, c_output));
            LPG_TRY(stream_writer_write_string(c_output, ";\n"));
            return success;

        case value_kind_enum_constructor:
            set_register_variable(
                state, input.literal.into, register_resource_ownership_borrows, input.literal.type_of);
            LPG_TRY(stream_writer_write_string(c_output, "/*enum constructor omitted*/\n"));
            return success;

        case value_kind_enum_element:
        {
            set_register_variable(
                state, input.literal.into, register_resource_ownership_borrows, input.literal.type_of);
            LPG_TRY(generate_type(input.literal.type_of, &state->standard_library, state->definitions,
                                  state->all_functions, state->all_interfaces, state->all_enums, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " const "));
            LPG_TRY(generate_register_name(input.literal.into, current_function, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = "));
            LPG_TRY(generate_value(input.literal.value_, input.literal.type_of, state, c_output));
            LPG_TRY(stream_writer_write_string(c_output, ";\n"));
            return success;
        }

        case value_kind_unit:
            set_register_variable(
                state, input.literal.into, register_resource_ownership_borrows, input.literal.type_of);
            state->standard_library.using_unit = true;
            LPG_TRY(stream_writer_write_string(c_output, "unit const "));
            LPG_TRY(generate_register_name(input.literal.into, current_function, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = "));
            LPG_TRY(generate_value(input.literal.value_, input.literal.type_of, state, c_output));
            LPG_TRY(stream_writer_write_string(c_output, ";\n"));
            return success;
        }
        LPG_UNREACHABLE();

    case instruction_tuple:
        return generate_tuple_variable(state, current_function, input.tuple_.result_type, input.tuple_.elements,
                                       input.tuple_.result, indentation, c_output);

    case instruction_instantiate_struct:
        return generate_instantiate_struct(state, current_function, input.instantiate_struct, indentation, c_output);

    case instruction_enum_construct:
    {
        set_register_variable(state, input.enum_construct.into, register_resource_ownership_owns,
                              type_from_enumeration(input.enum_construct.which.enumeration));
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(generate_type(type_from_enumeration(input.enum_construct.which.enumeration), &state->standard_library,
                              state->definitions, state->all_functions, state->all_interfaces, state->all_enums,
                              c_output));
        LPG_TRY(stream_writer_write_string(c_output, " const "));
        LPG_TRY(generate_register_name(input.enum_construct.into, current_function, c_output));
        LPG_TRY(stream_writer_write_string(c_output, " = {"));
        LPG_TRY(stream_writer_write_integer(c_output, integer_create(0, input.enum_construct.which.which)));
        LPG_TRY(stream_writer_write_string(c_output, ", {."));
        enumeration const enum_ = all_enums[input.enum_construct.which.enumeration];
        LPG_TRY(generate_struct_member_name(
            unicode_view_from_string(enum_.elements[input.enum_construct.which.which].name), c_output));
        LPG_TRY(stream_writer_write_string(c_output, " = "));
        LPG_TRY(generate_c_read_access(state, current_function, input.enum_construct.state, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "}};\n"));
        LPG_TRY(generate_add_reference_to_register(current_function, input.enum_construct.state,
                                                   enum_.elements[input.enum_construct.which.which].state, indentation,
                                                   state->all_functions, state->all_structs, c_output));
        return success;
    }

    case instruction_match:
    {
        set_register_variable(state, input.match.result, register_resource_ownership_owns, input.match.result_type);
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(generate_type(input.match.result_type, &state->standard_library, state->definitions,
                              state->all_functions, state->all_interfaces, state->all_enums, c_output));
        LPG_TRY(stream_writer_write_string(c_output, " "));
        LPG_TRY(generate_register_name(input.match.result, current_function, c_output));
        LPG_TRY(stream_writer_write_string(c_output, ";\n"));
        for (size_t i = 0; i < input.match.count; ++i)
        {
            LPG_TRY(indent(indentation, c_output));
            if (i > 0)
            {
                LPG_TRY(stream_writer_write_string(c_output, "else"));
            }
            if (i != (input.match.count - 1))
            {
                if (i > 0)
                {
                    LPG_TRY(stream_writer_write_string(c_output, " "));
                }
                LPG_TRY(stream_writer_write_string(c_output, "if ("));
                LPG_TRY(generate_c_read_access(state, current_function, input.match.key, c_output));
                ASSUME(state->registers[input.match.key].type_of.is_set);
                type const key_type = state->registers[input.match.key].type_of.value;
                if ((key_type.kind == type_kind_enumeration) && has_stateful_element(state->all_enums[key_type.enum_]))
                {
                    LPG_TRY(stream_writer_write_string(c_output, ".which == "));
                    switch (input.match.cases[i].kind)
                    {
                    case match_instruction_case_kind_value:
                        LPG_TRY(
                            generate_c_read_access(state, current_function, input.match.cases[i].key_value, c_output));
                        LPG_TRY(stream_writer_write_string(c_output, ".which"));
                        break;

                    case match_instruction_case_kind_stateful_enum:
                        LPG_TRY(stream_writer_write_integer(
                            c_output, integer_create(0, input.match.cases[i].stateful_enum.element)));
                        break;
                    }
                }
                else
                {
                    ASSUME(input.match.cases[i].kind == match_instruction_case_kind_value);
                    LPG_TRY(stream_writer_write_string(c_output, " == "));
                    LPG_TRY(generate_c_read_access(state, current_function, input.match.cases[i].key_value, c_output));
                }
                LPG_TRY(stream_writer_write_string(c_output, ")"));
            }
            LPG_TRY(stream_writer_write_string(c_output, "\n"));

            LPG_TRY(indent(indentation, c_output));
            LPG_TRY(stream_writer_write_string(c_output, "{\n"));
            size_t const previous_register_count = state->active_register_count;
            switch (input.match.cases[i].kind)
            {
            case match_instruction_case_kind_stateful_enum:
            {
                ASSUME(state->registers[input.match.key].type_of.is_set);
                ASSUME(state->registers[input.match.key].type_of.value.kind == type_kind_enumeration);
                enumeration const enum_ = all_enums[state->registers[input.match.key].type_of.value.enum_];
                type const state_type = enum_.elements[input.match.cases[i].stateful_enum.element].state;
                set_register_variable(
                    state, input.match.cases[i].stateful_enum.where, register_resource_ownership_owns, state_type);
                LPG_TRY(indent(indentation + 1, c_output));
                LPG_TRY(generate_type(state_type, &state->standard_library, state->definitions, state->all_functions,
                                      state->all_interfaces, state->all_enums, c_output));
                LPG_TRY(stream_writer_write_string(c_output, " const "));
                LPG_TRY(generate_register_name(input.match.cases[i].stateful_enum.where, current_function, c_output));
                LPG_TRY(stream_writer_write_string(c_output, " = "));
                LPG_TRY(generate_c_read_access(state, current_function, input.match.key, c_output));
                LPG_TRY(stream_writer_write_string(c_output, "."));
                LPG_TRY(generate_struct_member_name(
                    unicode_view_from_string(enum_.elements[input.match.cases[i].stateful_enum.element].name),
                    c_output));
                LPG_TRY(stream_writer_write_string(c_output, ";\n"));
                LPG_TRY(generate_add_reference_to_register(current_function, input.match.cases[i].stateful_enum.where,
                                                           state_type, indentation + 1, state->all_functions,
                                                           state->all_structs, c_output));
                ASSUME(state->active_register_count == (previous_register_count + 1));
                break;
            }

            case match_instruction_case_kind_value:
                break;
            }
            LPG_TRY(generate_sequence(state, all_functions, all_interfaces, current_function, current_function_result,
                                      input.match.cases[i].action, (indentation + 1), c_output));

            LPG_TRY(generate_free_registers(
                state, previous_register_count, indentation + 1, current_function, current_function_result, c_output));

            LPG_TRY(indent(indentation + 1, c_output));
            LPG_TRY(generate_register_name(input.match.result, current_function, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = "));
            LPG_TRY(generate_c_read_access(state, current_function, input.match.cases[i].value, c_output));
            LPG_TRY(stream_writer_write_string(c_output, ";\n"));

            LPG_TRY(indent(indentation, c_output));
            LPG_TRY(stream_writer_write_string(c_output, "}\n"));
        }
        return success;
    }

    case instruction_get_captures:
        set_register_meaning(state, input.captures,
                             optional_type_create_set(type_from_tuple_type(current_function->signature->captures)),
                             register_meaning_captures);
        return success;

    case instruction_lambda_with_captures:
    {
        type const function_type = type_from_lambda(lambda_type_create(input.lambda_with_captures.lambda));
        set_register_variable(state, input.lambda_with_captures.into, register_resource_ownership_owns, function_type);
        tuple_type const captures = all_functions[input.lambda_with_captures.lambda].signature->captures;
        for (size_t i = 0; i < captures.length; ++i)
        {
            LPG_TRY(generate_add_reference_to_register(current_function, input.lambda_with_captures.captures[i],
                                                       captures.elements[i], indentation, state->all_functions,
                                                       state->all_structs, c_output));
        }
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(generate_type(function_type, &state->standard_library, state->definitions, state->all_functions,
                              state->all_interfaces, state->all_enums, c_output));
        LPG_TRY(stream_writer_write_string(c_output, " const "));
        LPG_TRY(generate_register_name(input.lambda_with_captures.into, current_function, c_output));
        LPG_TRY(stream_writer_write_string(c_output, " = "));
        LPG_TRY(generate_tuple_initializer(
            state, current_function, captures.length, input.lambda_with_captures.captures, c_output));
        LPG_TRY(stream_writer_write_string(c_output, ";\n"));
        return success;
    }
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_free_enumeration(standard_library_usage *const standard_library,
                                                   unicode_view const freed, enumeration const what,
                                                   checked_function const *const all_functions,
                                                   structure const *const all_structures,
                                                   enumeration const *const all_enums, size_t const indentation,
                                                   stream_writer const c_output)
{
    if (!has_stateful_element(what))
    {
        return success;
    }
    LPG_TRY(indent(indentation, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "switch ("));
    LPG_TRY(stream_writer_write_unicode_view(c_output, freed));
    LPG_TRY(stream_writer_write_string(c_output, ".which)\n"));
    LPG_TRY(indent(indentation, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "{\n"));
    for (enum_element_id i = 0; i < what.size; ++i)
    {
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "case "));
        LPG_TRY(stream_writer_write_integer(c_output, integer_create(0, i)));
        LPG_TRY(stream_writer_write_string(c_output, ":\n"));
        memory_writer name_buffer = {NULL, 0, 0};
        LPG_TRY(stream_writer_write_unicode_view(memory_writer_erase(&name_buffer), freed));
        LPG_TRY(stream_writer_write_string(memory_writer_erase(&name_buffer), "."));
        LPG_TRY(generate_struct_member_name(
            unicode_view_from_string(what.elements[i].name), memory_writer_erase(&name_buffer)));
        LPG_TRY(generate_free(standard_library, memory_writer_content(name_buffer), what.elements[i].state,
                              all_functions, all_structures, all_enums, indentation + 1, c_output));
        memory_writer_free(&name_buffer);
        LPG_TRY(indent(indentation + 1, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "break;\n"));
    }
    LPG_TRY(indent(indentation, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "}\n"));
    return success;
}

static success_indicator generate_free(standard_library_usage *const standard_library, unicode_view const freed,
                                       type const what, checked_function const *const all_functions,
                                       structure const *const all_structures, enumeration const *const all_enums,
                                       size_t const indentation, stream_writer const c_output)
{
    switch (what.kind)
    {
    case type_kind_method_pointer:
        return generate_free(standard_library, freed, type_from_interface(what.method_pointer.interface_),
                             all_functions, all_structures, all_enums, indentation, c_output);

    case type_kind_interface:
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(stream_writer_write_unicode_view(c_output, freed));
        LPG_TRY(stream_writer_write_string(c_output, ".vtable->_add_reference("));
        LPG_TRY(stream_writer_write_unicode_view(c_output, freed));
        LPG_TRY(stream_writer_write_string(c_output, ".self, -1);\n"));
        return success;

    case type_kind_string_ref:
        standard_library->using_string_ref = true;
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "string_ref_free(&"));
        LPG_TRY(stream_writer_write_unicode_view(c_output, freed));
        LPG_TRY(stream_writer_write_string(c_output, ");\n"));
        return success;

    case type_kind_structure:
    {
        structure const struct_ = all_structures[what.structure_];
        for (struct_member_id i = 0; i < struct_.count; ++i)
        {
            memory_writer name_buffer = {NULL, 0, 0};
            LPG_TRY(stream_writer_write_unicode_view(memory_writer_erase(&name_buffer), freed));
            LPG_TRY(stream_writer_write_string(memory_writer_erase(&name_buffer), "."));
            LPG_TRY(generate_struct_member_name(
                unicode_view_from_string(struct_.members[i].name), memory_writer_erase(&name_buffer)));
            LPG_TRY(generate_free(standard_library, memory_writer_content(name_buffer), struct_.members[i].what,
                                  all_functions, all_structures, all_enums, indentation, c_output));
            memory_writer_free(&name_buffer);
        }
        return success;
    }

    case type_kind_tuple:
        for (struct_member_id i = 0; i < what.tuple_.length; ++i)
        {
            memory_writer name_buffer = {NULL, 0, 0};
            LPG_TRY(stream_writer_write_unicode_view(memory_writer_erase(&name_buffer), freed));
            LPG_TRY(stream_writer_write_string(memory_writer_erase(&name_buffer), "."));
            LPG_TRY(generate_tuple_element_name(i, memory_writer_erase(&name_buffer)));
            LPG_TRY(generate_free(standard_library, memory_writer_content(name_buffer), what.tuple_.elements[i],
                                  all_functions, all_structures, all_enums, indentation, c_output));
            memory_writer_free(&name_buffer);
        }
        return success;

    case type_kind_enum_constructor:
    case type_kind_function_pointer:
    case type_kind_type:
    case type_kind_unit:
    case type_kind_integer_range:
        return success;

    case type_kind_enumeration:
        return generate_free_enumeration(standard_library, freed, all_enums[what.enum_], all_functions, all_structures,
                                         all_enums, indentation, c_output);

    case type_kind_lambda:
    {
        checked_function const lambda_function = all_functions[what.lambda.lambda];
        LPG_TRY(generate_free(standard_library, freed, type_from_tuple_type(lambda_function.signature->captures),
                              all_functions, all_structures, all_enums, indentation, c_output));
        return success;
    }
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_sequence(c_backend_state *state, checked_function const *const all_functions,
                                           interface const *const all_interfaces,
                                           checked_function const *const current_function,
                                           register_id const current_function_result,
                                           instruction_sequence const sequence, size_t const indentation,
                                           stream_writer const c_output)
{
    size_t const previously_active_registers = state->active_register_count;
    for (size_t i = 0; i < sequence.length; ++i)
    {
        LPG_TRY(generate_instruction(state, all_functions, all_interfaces, state->all_enums, current_function,
                                     current_function_result, sequence.elements[i], indentation, c_output));
    }
    return generate_free_registers(
        state, previously_active_registers, indentation, current_function, current_function_result, c_output);
}

static success_indicator generate_function_body(checked_function const current_function,
                                                checked_function const *const all_functions,
                                                interface const *const all_interfaces,
                                                structure const *const all_structures,
                                                enumeration const *const all_enums, stream_writer const c_output,
                                                standard_library_usage *standard_library,
                                                type_definitions *const definitions, bool const return_0)
{
    LPG_TRY(stream_writer_write_string(c_output, "{\n"));

    c_backend_state state = {allocate_array(current_function.number_of_registers, sizeof(*state.registers)),
                             *standard_library,
                             NULL,
                             0,
                             all_functions,
                             all_interfaces,
                             definitions,
                             all_structures,
                             all_enums};
    for (size_t j = 0; j < current_function.number_of_registers; ++j)
    {
        state.registers[j].meaning = register_meaning_nothing;
        state.registers[j].ownership = register_resource_ownership_owns;
    }

    register_id next_free_register = 0;
    if (current_function.signature->self.is_set)
    {
        set_register_argument(
            &state, next_free_register, register_resource_ownership_borrows, current_function.signature->self.value);
        LPG_TRY(stream_writer_write_string(c_output, "    "));
        LPG_TRY(generate_type(current_function.signature->self.value, standard_library, definitions, all_functions,
                              all_interfaces, all_enums, c_output));
        LPG_TRY(stream_writer_write_string(c_output, " const "));
        LPG_TRY(generate_register_name(next_free_register, &current_function, c_output));
        LPG_TRY(stream_writer_write_string(c_output, " = *("));
        LPG_TRY(generate_type(current_function.signature->self.value, standard_library, definitions, all_functions,
                              all_interfaces, all_enums, c_output));
        LPG_TRY(stream_writer_write_string(c_output, " const *)self;\n"));
        ++next_free_register;
    }

    for (register_id i = 0; i < current_function.signature->parameters.length; ++i)
    {
        type const parameter = current_function.signature->parameters.elements[i];
        set_register_argument(&state, next_free_register, register_resource_ownership_borrows, parameter);
        ++next_free_register;
    }
    LPG_TRY(generate_sequence(&state, all_functions, all_interfaces, &current_function, current_function.return_value,
                              current_function.body, 1, c_output));

    if (!return_0)
    {
        LPG_TRY(generate_add_reference_for_return_value(&state, &current_function, current_function.return_value, 1,
                                                        state.all_functions, state.all_structs, c_output));
    }

    LPG_TRY(stream_writer_write_string(c_output, "    return "));
    if (return_0)
    {
        LPG_TRY(stream_writer_write_string(c_output, "0"));
    }
    else
    {
        LPG_TRY(generate_c_read_access(&state, &current_function, current_function.return_value, c_output));
    }
    LPG_TRY(stream_writer_write_string(c_output, ";\n"));
    LPG_TRY(stream_writer_write_string(c_output, "}\n"));
    *standard_library = state.standard_library;
    deallocate(state.registers);
    if (state.active_registers)
    {
        deallocate(state.active_registers);
    }
    return success;
}

static success_indicator
generate_function_declaration(function_id const id, function_pointer const signature,
                              standard_library_usage *const standard_library, type_definitions *const definitions,
                              checked_function const *const all_functions, interface const *const all_interfaces,
                              enumeration const *const all_enums, stream_writer const program_defined_writer)
{
    LPG_TRY(stream_writer_write_string(program_defined_writer, "static "));
    LPG_TRY(generate_type(signature.result, standard_library, definitions, all_functions, all_interfaces, all_enums,
                          program_defined_writer));
    LPG_TRY(stream_writer_write_string(program_defined_writer, " "));
    LPG_TRY(generate_function_name(id, program_defined_writer));
    LPG_TRY(stream_writer_write_string(program_defined_writer, "("));
    bool add_comma = false;
    if (signature.self.is_set)
    {
        LPG_TRY(stream_writer_write_string(program_defined_writer, "void *const self"));
        add_comma = true;
    }
    else if (signature.captures.length > 0)
    {
        add_comma = true;
        LPG_TRY(generate_type(type_from_tuple_type(signature.captures), standard_library, definitions, all_functions,
                              all_interfaces, all_enums, program_defined_writer));
        LPG_TRY(stream_writer_write_string(program_defined_writer, " const *const captures"));
    }
    else if (signature.parameters.length == 0)
    {
        LPG_TRY(stream_writer_write_string(program_defined_writer, "void"));
    }
    for (register_id j = 0; j < signature.parameters.length; ++j)
    {
        if (add_comma)
        {
            LPG_TRY(stream_writer_write_string(program_defined_writer, ", "));
        }
        else
        {
            add_comma = true;
        }
        LPG_TRY(generate_type(signature.parameters.elements[j], standard_library, definitions, all_functions,
                              all_interfaces, all_enums, program_defined_writer));
        LPG_TRY(stream_writer_write_string(program_defined_writer, " const "));
        LPG_TRY(generate_register_name(signature.self.is_set + j, all_functions + id, program_defined_writer));
    }
    LPG_TRY(stream_writer_write_string(program_defined_writer, ")"));
    return success;
}

success_indicator generate_c(checked_program const program, stream_writer const c_output)
{
    memory_writer program_defined = {NULL, 0, 0};
    stream_writer program_defined_writer = memory_writer_erase(&program_defined);

    standard_library_usage standard_library = {false, false, false, false, false, false};

    type_definitions definitions = {NULL, 0, 0};

    for (interface_id i = 0; i < program.interface_count; ++i)
    {
        LPG_TRY_GOTO(generate_interface_vtable_definition(i, &standard_library, &definitions, program.functions,
                                                          program.interfaces, program.enums, program_defined_writer),
                     fail);
    }

    for (struct_id i = 0; i < program.struct_count; ++i)
    {
        LPG_TRY_GOTO(
            generate_struct_definition(i, program.structs[i], &standard_library, &definitions, program.functions,
                                       program.interfaces, program.enums, program_defined_writer),
            fail);
    }

    for (enum_id i = 0; i < program.enum_count; ++i)
    {
        if (!has_stateful_element(program.enums[i]))
        {
            continue;
        }
        LPG_TRY_GOTO(generate_stateful_enum_definition(i, &standard_library, &definitions, program.functions,
                                                       program.interfaces, program.enums, program_defined_writer),
                     fail);
    }

    for (function_id i = 1; i < program.function_count; ++i)
    {
        checked_function const current_function = program.functions[i];
        LPG_TRY_GOTO(
            generate_function_declaration(i, *current_function.signature, &standard_library, &definitions,
                                          program.functions, program.interfaces, program.enums, program_defined_writer),
            fail);
        LPG_TRY_GOTO(stream_writer_write_string(program_defined_writer, ";\n"), fail);
    }

    for (interface_id i = 0; i < program.interface_count; ++i)
    {
        interface const interface_ = program.interfaces[i];
        for (size_t k = 0; k < interface_.implementation_count; ++k)
        {
            LPG_TRY_GOTO(generate_interface_impl_definition(implementation_ref_create(i, k), &definitions,
                                                            program.functions, program.interfaces, program.structs,
                                                            program.enums, &standard_library, program_defined_writer),
                         fail);
        }
    }

    for (function_id i = 1; i < program.function_count; ++i)
    {
        checked_function const current_function = program.functions[i];
        LPG_TRY_GOTO(
            generate_function_declaration(i, *current_function.signature, &standard_library, &definitions,
                                          program.functions, program.interfaces, program.enums, program_defined_writer),
            fail);
        LPG_TRY_GOTO(stream_writer_write_string(program_defined_writer, "\n"), fail);
        LPG_TRY_GOTO(
            generate_function_body(current_function, program.functions, program.interfaces, program.structs,
                                   program.enums, program_defined_writer, &standard_library, &definitions, false),
            fail);
    }

    LPG_TRY_GOTO(stream_writer_write_string(program_defined_writer, "int main(void)\n"), fail);
    LPG_TRY_GOTO(generate_function_body(program.functions[0], program.functions, program.interfaces, program.structs,
                                        program.enums, program_defined_writer, &standard_library, &definitions, true),
                 fail);

    if (standard_library.using_unit)
    {
        LPG_TRY_GOTO(stream_writer_write_string(c_output, "#include <lpg_std_unit.h>\n"), fail);
    }
    if (standard_library.using_assert)
    {
        LPG_TRY_GOTO(stream_writer_write_string(c_output, "#include <lpg_std_assert.h>\n"), fail);
    }
    if (standard_library.using_string_ref)
    {
        LPG_TRY_GOTO(stream_writer_write_string(c_output, "#include <lpg_std_string.h>\n"), fail);
    }
    if (standard_library.using_stdint)
    {
        LPG_TRY_GOTO(stream_writer_write_string(c_output, "#include <stdint.h>\n"), fail);
    }
    if (standard_library.using_integer)
    {
        LPG_TRY_GOTO(stream_writer_write_string(c_output, "#include <lpg_std_integer.h>\n"), fail);
    }
    if (standard_library.using_boolean)
    {
        LPG_TRY_GOTO(stream_writer_write_string(c_output, "#include <lpg_std_boolean.h>\n"), fail);
    }
    LPG_TRY_GOTO(stream_writer_write_string(c_output, "#include <stddef.h>\n"
                                                      "typedef size_t stateless_enum;\n"),
                 fail);

    for (interface_id i = 0; i < program.interface_count; ++i)
    {
        LPG_TRY_GOTO(generate_interface_reference_definition(i, c_output), fail);
    }

    for (struct_id i = 0; i < program.struct_count; ++i)
    {
        LPG_TRY(stream_writer_write_string(c_output, "typedef struct "));
        LPG_TRY_GOTO(generate_struct_name(i, c_output), fail);
        LPG_TRY(stream_writer_write_string(c_output, " "));
        LPG_TRY_GOTO(generate_struct_name(i, c_output), fail);
        LPG_TRY(stream_writer_write_string(c_output, ";\n"));
    }

    for (enum_id i = 0; i < program.enum_count; ++i)
    {
        if (!has_stateful_element(program.enums[i]))
        {
            continue;
        }
        LPG_TRY(stream_writer_write_string(c_output, "typedef struct "));
        LPG_TRY_GOTO(generate_stateful_enum_name(i, c_output), fail);
        LPG_TRY(stream_writer_write_string(c_output, " "));
        LPG_TRY_GOTO(generate_stateful_enum_name(i, c_output), fail);
        LPG_TRY(stream_writer_write_string(c_output, ";\n"));
    }

    ASSUME(definitions.count == definitions.next_order);
    {
        size_t *const ordered_definitions = type_definitions_sort_by_order(definitions);
        for (size_t i = 0; i < definitions.count; ++i)
        {
            size_t const definition_index = ordered_definitions[i];
            LPG_TRY_GOTO(stream_writer_write_unicode_view(
                             c_output, unicode_view_from_string(definitions.elements[definition_index].definition)),
                         fail);
        }
        if (ordered_definitions)
        {
            deallocate(ordered_definitions);
        }
    }

    LPG_TRY_GOTO(stream_writer_write_bytes(c_output, program_defined.data, program_defined.used), fail);
    type_definitions_free(definitions);
    memory_writer_free(&program_defined);
    return success;

fail:
    type_definitions_free(definitions);
    memory_writer_free(&program_defined);
    return failure;
}
