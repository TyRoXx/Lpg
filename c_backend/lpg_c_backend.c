#include "lpg_allocate.h"
#include "lpg_assert.h"
#include "lpg_c_backend.h"
#include "lpg_instruction.h"
#include "lpg_structure_member.h"
#include <string.h>

typedef struct standard_library_usage
{
    bool using_string_ref;
    bool using_assert;
    bool using_unit;
    bool using_stdint;
    bool using_integer;
    bool using_boolean;
    bool using_stdlib;
    bool using_c_assert;
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

static success_indicator generate_free(standard_library_usage *const standard_library, unicode_view const freed,
                                       type const what, checked_program const *const program, size_t const indentation,
                                       stream_writer const c_output);

typedef struct array_vtable
{
    interface_id array_type;
    unicode_string definition;
} array_vtable;

static array_vtable array_vtable_create(interface_id array_type, unicode_string definition)
{
    array_vtable const result = {array_type, definition};
    return result;
}

static void array_vtable_free(array_vtable const freed)
{
    unicode_string_free(&freed.definition);
}

typedef struct array_vtable_cache
{
    array_vtable *array_vtables;
    size_t array_vtable_count;
} array_vtable_cache;

static void array_vtable_cache_free(array_vtable_cache const freed)
{
    for (size_t i = 0; i < freed.array_vtable_count; ++i)
    {
        array_vtable_free(freed.array_vtables[i]);
    }
    if (freed.array_vtables)
    {
        deallocate(freed.array_vtables);
    }
}

static success_indicator generate_interface_vtable_name(interface_id const generated, stream_writer const c_output)
{
    LPG_TRY(stream_writer_write_string(c_output, "interface_vtable_"));
    return stream_writer_write_integer(c_output, integer_create(0, generated));
}

typedef struct c_backend_state
{
    register_state *registers;
    standard_library_usage standard_library;
    register_id *active_registers;
    size_t active_register_count;
    type_definitions *definitions;
    checked_program const *program;
    array_vtable_cache *array_vtables;
} c_backend_state;

static success_indicator generate_type(type const generated, standard_library_usage *const standard_library,
                                       type_definitions *const definitions, checked_program const *const program,
                                       stream_writer const c_output);

static success_indicator generate_array_vtable_name(stream_writer const c_output, interface_id const array_interface)
{
    LPG_TRY(stream_writer_write_string(c_output, "array_vtable_"));
    return stream_writer_write_integer(c_output, integer_create(0, array_interface));
}

static success_indicator generate_array_impl_name(stream_writer const c_output, interface_id const array_interface)
{
    LPG_TRY(stream_writer_write_string(c_output, "array_impl_"));
    return stream_writer_write_integer(c_output, integer_create(0, array_interface));
}

static success_indicator generate_add_reference(unicode_view const value, type const what, size_t const indentation,
                                                checked_program const *const program, stream_writer const c_output);

static success_indicator generate_array_vtable(stream_writer const c_output, interface_id const array_interface,
                                               type const element_type, standard_library_usage *const standard_library,
                                               type_definitions *const definitions,
                                               checked_program const *const program)
{
    LPG_TRY(stream_writer_write_string(c_output, "typedef struct "));
    LPG_TRY(generate_array_impl_name(c_output, array_interface));
    LPG_TRY(stream_writer_write_string(c_output, "\n{\n"
                                                 "    "));
    LPG_TRY(generate_type(element_type, standard_library, definitions, program, c_output));
    LPG_TRY(stream_writer_write_string(c_output, " *elements;\n"
                                                 "    size_t used;\n"
                                                 "    size_t allocated;\n"
                                                 "    ptrdiff_t references;\n"
                                                 "} "));
    LPG_TRY(generate_array_impl_name(c_output, array_interface));
    LPG_TRY(stream_writer_write_string(c_output, ";\n"));
    LPG_TRY(stream_writer_write_string(c_output, "static void "));
    LPG_TRY(generate_interface_vtable_name(array_interface, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "_add_reference(void *self, ptrdiff_t difference)\n"
                                                 "{\n"
                                                 "    "));
    LPG_TRY(generate_array_impl_name(c_output, array_interface));
    LPG_TRY(stream_writer_write_string(c_output, " * const impl = self;\n"
                                                 "    impl->references += difference;\n"
                                                 "    if (impl->references > 0)\n"
                                                 "    {\n"
                                                 "        return;\n"
                                                 "    }\n"));
    standard_library->using_c_assert = true;
    LPG_TRY(stream_writer_write_string(c_output, "    assert(impl->references == 0);\n"
                                                 "    for (size_t i = 0, c = impl->used; i < c; ++i)\n"
                                                 "    {\n"));
    LPG_TRY(generate_free(
        standard_library, unicode_view_from_c_str("impl->elements[i]"), element_type, program, 2, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "    }\n"
                                                 "    free(impl->elements);\n"
                                                 "    free(impl);\n"
                                                 "}\n"));

    LPG_TRY(stream_writer_write_string(c_output, "static uint64_t "));
    LPG_TRY(generate_interface_vtable_name(array_interface, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "_size(void *self)\n"
                                                 "{\n"
                                                 "    "));
    LPG_TRY(generate_array_impl_name(c_output, array_interface));
    LPG_TRY(stream_writer_write_string(c_output, " * const impl = self;\n"
                                                 "    return impl->used;\n"
                                                 "}\n"));

    type const load_result = program->interfaces[array_interface].methods[1].result;
    ASSUME(load_result.kind == type_kind_enumeration);
    LPG_TRY(stream_writer_write_string(c_output, "static "));
    LPG_TRY(generate_type(load_result, standard_library, definitions, program, c_output));
    LPG_TRY(stream_writer_write_string(c_output, " "));
    LPG_TRY(generate_interface_vtable_name(array_interface, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "_load(void *self, uint64_t const index)\n"
                                                 "{\n"
                                                 "    "));
    LPG_TRY(generate_array_impl_name(c_output, array_interface));
    LPG_TRY(stream_writer_write_string(c_output, " * const impl = self;\n"
                                                 "    "));
    LPG_TRY(generate_type(load_result, standard_library, definitions, program, c_output));
    LPG_TRY(stream_writer_write_string(c_output, " result;\n"
                                                 "    result.which = 1;\n"
                                                 "    if (index < impl->used)\n"
                                                 "    {\n"
                                                 "        result.which = 0;\n"
                                                 "        result.e_some = impl->elements[index];\n"));
    LPG_TRY(generate_add_reference(unicode_view_from_c_str("result.e_some"), element_type, 2, program, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "    }\n"
                                                 "    return result;\n"
                                                 "}\n"));

    LPG_TRY(stream_writer_write_string(c_output, "static stateless_enum "));
    LPG_TRY(generate_interface_vtable_name(array_interface, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "_store(void *self, uint64_t const index, "));
    LPG_TRY(generate_type(element_type, standard_library, definitions, program, c_output));
    LPG_TRY(stream_writer_write_string(c_output, " element)\n"
                                                 "{\n"
                                                 "    "));
    LPG_TRY(generate_array_impl_name(c_output, array_interface));
    LPG_TRY(stream_writer_write_string(c_output, " * const impl = self;\n"
                                                 "    if (index >= impl->used)\n"
                                                 "    {\n"
                                                 "        return 0;\n"
                                                 "    }\n"));
    LPG_TRY(generate_add_reference(unicode_view_from_c_str("element"), element_type, 1, program, c_output));
    LPG_TRY(generate_free(
        standard_library, unicode_view_from_c_str("impl->elements[index]"), element_type, program, 1, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "    impl->elements[index] = element;\n"
                                                 "    return 1;\n"
                                                 "}\n"));

    LPG_TRY(stream_writer_write_string(c_output, "static stateless_enum "));
    LPG_TRY(generate_interface_vtable_name(array_interface, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "_append(void *self, "));
    LPG_TRY(generate_type(element_type, standard_library, definitions, program, c_output));
    LPG_TRY(stream_writer_write_string(c_output, " element)\n"
                                                 "{\n"
                                                 "    "));
    LPG_TRY(generate_array_impl_name(c_output, array_interface));
    LPG_TRY(stream_writer_write_string(
        c_output, " * const impl = self;\n"
                  "    if (impl->allocated == impl->used)\n"
                  "    {\n"
                  "        if ((SIZE_MAX / 2) < impl->used)\n"
                  "        {\n"
                  "            // overflow\n"
                  "            return 0;\n"
                  "        }\n"
                  "        size_t new_capacity = (impl->used * 2);\n"
                  "        if (new_capacity == 0)\n"
                  "        {\n"
                  "            new_capacity = 1;\n"
                  "        }\n"
                  "        if ((SIZE_MAX / sizeof(*impl->elements)) < new_capacity)\n"
                  "        {\n"
                  "            // overflow\n"
                  "            return 0;\n"
                  "        }\n"
                  "        impl->elements = realloc(impl->elements, (new_capacity * sizeof(*impl->elements)));\n"
                  "        impl->allocated = new_capacity;\n"
                  "    }\n"
                  "    impl->elements[impl->used] = element;\n"));
    LPG_TRY(generate_add_reference(unicode_view_from_c_str("element"), element_type, 1, program, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "    impl->used += 1;\n"
                                                 "    return 1;\n"
                                                 "}\n"));

    LPG_TRY(stream_writer_write_string(c_output, "static "));
    LPG_TRY(generate_interface_vtable_name(array_interface, c_output));
    LPG_TRY(stream_writer_write_string(c_output, " const "));
    LPG_TRY(generate_array_vtable_name(c_output, array_interface));
    LPG_TRY(stream_writer_write_string(c_output, " = {"));

    LPG_TRY(generate_interface_vtable_name(array_interface, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "_add_reference, "));

    LPG_TRY(generate_interface_vtable_name(array_interface, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "_size, "));

    LPG_TRY(generate_interface_vtable_name(array_interface, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "_load, "));

    LPG_TRY(generate_interface_vtable_name(array_interface, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "_store, "));

    LPG_TRY(generate_interface_vtable_name(array_interface, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "_append"));

    LPG_TRY(stream_writer_write_string(c_output, "};\n"));
    return success_yes;
}

static void require_array_vtable(array_vtable_cache *const state, interface_id const array_interface,
                                 type const element_type, standard_library_usage *const standard_library,
                                 type_definitions *const definitions, checked_program const *const program)
{
    for (size_t i = 0; i < state->array_vtable_count; ++i)
    {
        if (state->array_vtables[i].array_type == array_interface)
        {
            return;
        }
    }
    memory_writer buffer = {NULL, 0, 0};
    stream_writer const writer = memory_writer_erase(&buffer);
    switch (generate_array_vtable(writer, array_interface, element_type, standard_library, definitions, program))
    {
    case success_yes:
        break;

    case success_no:
        LPG_TO_DO();
    }
    state->array_vtables =
        reallocate_array(state->array_vtables, (state->array_vtable_count + 1), sizeof(*state->array_vtables));
    unicode_string const definition = {buffer.data, buffer.used};
    state->array_vtables[state->array_vtable_count] = array_vtable_create(array_interface, definition);
    state->array_vtable_count += 1;
}

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
    return success_yes;
}

static success_indicator generate_integer(integer const generated, stream_writer const c_output)
{
    char buffer[64];
    char const *const formatted = integer_format(generated, lower_case_digits, 10, buffer, sizeof(buffer));
    LPG_TRY(stream_writer_write_bytes(c_output, formatted, (size_t)((buffer + sizeof(buffer)) - formatted)));
    return success_yes;
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
    return success_yes;
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
    return success_yes;
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
    return success_yes;
}

static success_indicator generate_c_function_pointer(type const generated,
                                                     standard_library_usage *const standard_library,
                                                     type_definitions *const definitions,
                                                     checked_program const *const program, stream_writer const c_output)
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
    LPG_TRY(
        generate_type(generated.function_pointer_->result, standard_library, definitions, program, definition_writer));
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
                              program, definition_writer));
    }
    LPG_TRY(stream_writer_write_string(definition_writer, ");\n"));
    type_definition *const new_definition = definitions->elements + definition_index;
    new_definition->definition.data = definition_buffer.data;
    new_definition->definition.length = definition_buffer.used;
    new_definition->order = definitions->next_order++;
    return stream_writer_write_unicode_view(c_output, unicode_view_from_string(name));
}

static success_indicator generate_interface_vtable_definition(interface_id const generated,
                                                              standard_library_usage *const standard_library,
                                                              type_definitions *const definitions,
                                                              checked_program const *const program,
                                                              stream_writer const c_output)
{
    LPG_TRY(stream_writer_write_string(c_output, "typedef struct "));
    LPG_TRY(generate_interface_vtable_name(generated, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "\n"));
    LPG_TRY(stream_writer_write_string(c_output, "{\n"));

    LPG_TRY(indent(1, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "void (*_add_reference)(void *, ptrdiff_t);\n"));

    lpg_interface const our_interface = program->interfaces[generated];
    for (function_id i = 0; i < our_interface.method_count; ++i)
    {
        LPG_TRY(indent(1, c_output));
        method_description const method = our_interface.methods[i];
        LPG_TRY(generate_type(method.result, standard_library, definitions, program, c_output));
        LPG_TRY(stream_writer_write_string(c_output, " (*"));
        LPG_TRY(escape_identifier(unicode_view_from_string(method.name), c_output));
        LPG_TRY(stream_writer_write_string(c_output, ")(void *"));
        for (size_t k = 0; k < method.parameters.length; ++k)
        {
            LPG_TRY(stream_writer_write_string(c_output, ", "));
            LPG_TRY(generate_type(method.parameters.elements[k], standard_library, definitions, program, c_output));
        }
        LPG_TRY(stream_writer_write_string(c_output, ");\n"));
    }

    LPG_TRY(stream_writer_write_string(c_output, "} "));
    /*no newline here because clang-format will replace it with a
     * space*/

    LPG_TRY(generate_interface_vtable_name(generated, c_output));
    LPG_TRY(stream_writer_write_string(c_output, ";\n"));
    return success_yes;
}

static success_indicator generate_interface_reference_name(interface_id const generated, stream_writer const c_output)
{
    LPG_TRY(stream_writer_write_string(c_output, "interface_reference_"));
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
    return success_yes;
}

static success_indicator generate_interface_impl_name(implementation_ref const generated, stream_writer const c_output)
{
    LPG_TRY(stream_writer_write_string(c_output, "interface_impl_"));
    LPG_TRY(stream_writer_write_integer(c_output, integer_create(0, generated.implementation_index)));
    LPG_TRY(stream_writer_write_string(c_output, "_for_"));
    LPG_TRY(stream_writer_write_integer(c_output, integer_create(0, generated.target)));
    return success_yes;
}

static success_indicator generate_interface_impl_add_reference(implementation_ref const generated,
                                                               stream_writer const c_output)
{
    LPG_TRY(stream_writer_write_string(c_output, "add_reference_"));
    LPG_TRY(stream_writer_write_integer(c_output, integer_create(0, generated.implementation_index)));
    LPG_TRY(stream_writer_write_string(c_output, "_for_"));
    LPG_TRY(stream_writer_write_integer(c_output, integer_create(0, generated.target)));
    return success_yes;
}

static success_indicator generate_interface_impl_definition(implementation_ref const generated,
                                                            type_definitions *const definitions,
                                                            checked_program const *const program,
                                                            standard_library_usage *const standard_library,
                                                            stream_writer const c_output)
{
    implementation_entry const impl =
        program->interfaces[generated.target].implementations[generated.implementation_index];

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
        LPG_TRY(generate_type(impl.self, standard_library, definitions, program, freed_writer));
        LPG_TRY(stream_writer_write_string(freed_writer, " *)self)"));
        LPG_TRY(generate_free(
            standard_library, unicode_view_create(freed.data, freed.used), impl.self, program, 2, c_output));
        memory_writer_free(&freed);
    }
    LPG_TRY(indent(2, c_output));
    standard_library->using_stdlib = true;
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
    return success_yes;
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
    return success_yes;
}

static success_indicator generate_stateful_enum_name(enum_id const generated, stream_writer const c_output)
{
    LPG_TRY(stream_writer_write_string(c_output, "enum_"));
    LPG_TRY(stream_writer_write_integer(c_output, integer_create(0, generated)));
    return success_yes;
}

static success_indicator generate_stateful_enum_definition(enum_id const id,
                                                           standard_library_usage *const standard_library,
                                                           type_definitions *const definitions,
                                                           checked_program const *const program,
                                                           stream_writer const c_output)
{
    enumeration const generated = program->enums[id];
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
        LPG_TRY(generate_type(member.state, standard_library, definitions, program, c_output));
        LPG_TRY(stream_writer_write_string(c_output, " "));
        LPG_TRY(generate_struct_member_name(unicode_view_from_string(member.name), c_output));
        LPG_TRY(stream_writer_write_string(c_output, ";\n"));
    }
    LPG_TRY(indent(1, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "};\n"));
    LPG_TRY(stream_writer_write_string(c_output, "};\n"));
    return success_yes;
}

static success_indicator generate_type(type const generated, standard_library_usage *const standard_library,
                                       type_definitions *const definitions, checked_program const *const program,
                                       stream_writer const c_output)
{
    switch (generated.kind)
    {
    case type_kind_interface:
        return generate_interface_reference_name(generated.interface_, c_output);

    case type_kind_lambda:
    {
        function_pointer const *const signature = program->functions[generated.lambda.lambda].signature;
        if (signature->captures.length)
        {
            return generate_type(
                type_from_tuple_type(signature->captures), standard_library, definitions, program, c_output);
        }
        return generate_c_function_pointer(
            type_from_function_pointer(signature), standard_library, definitions, program, c_output);
    }

    case type_kind_function_pointer:
        if (generated.function_pointer_->captures.length)
        {
            return generate_type(type_from_tuple_type(generated.function_pointer_->captures), standard_library,
                                 definitions, program, c_output);
        }
        return generate_c_function_pointer(generated, standard_library, definitions, program, c_output);

    case type_kind_unit:
        standard_library->using_unit = true;
        return stream_writer_write_string(c_output, "unit");

    case type_kind_string_ref:
        standard_library_usage_use_string_ref(standard_library);
        return stream_writer_write_string(c_output, "string_ref");

    case type_kind_enumeration:
        if (has_stateful_element(program->enums[generated.enum_]))
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
            LPG_TRY(generate_type(generated, standard_library, definitions, program, definition_writer));
            LPG_TRY(stream_writer_write_string(definition_writer, "\n"));
            LPG_TRY(stream_writer_write_string(definition_writer, "{\n"));
            for (struct_member_id i = 0; i < generated.tuple_.length; ++i)
            {
                LPG_TRY(indent(1, definition_writer));
                LPG_TRY(generate_type(
                    generated.tuple_.elements[i], standard_library, definitions, program, definition_writer));
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
        LPG_TRY(generate_type(generated, standard_library, definitions, program, definition_writer));
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
    case type_kind_generic_interface:
    case type_kind_generic_enum:
        standard_library->using_unit = true;
        return stream_writer_write_string(c_output, "unit");

    case type_kind_enum_constructor:
    case type_kind_method_pointer:
        LPG_TO_DO();

    case type_kind_structure:
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
        structure const generated_structure = program->structs[generated.structure_];
        if (generated_structure.count > 0)
        {
            LPG_TRY(stream_writer_write_string(definition_writer, "typedef struct "));
            LPG_TRY(generate_type(generated, standard_library, definitions, program, definition_writer));
            LPG_TRY(stream_writer_write_string(definition_writer, "\n"));
            LPG_TRY(stream_writer_write_string(definition_writer, "{\n"));
            for (struct_member_id i = 0; i < generated_structure.count; ++i)
            {
                LPG_TRY(indent(1, definition_writer));
                LPG_TRY(generate_type(
                    generated_structure.members[i].what, standard_library, definitions, program, definition_writer));
                LPG_TRY(stream_writer_write_string(definition_writer, " "));
                LPG_TRY(generate_struct_member_name(
                    unicode_view_from_string(generated_structure.members[i].name), definition_writer));
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
        LPG_TRY(generate_type(generated, standard_library, definitions, program, definition_writer));
        LPG_TRY(stream_writer_write_string(definition_writer, ";\n"));
        type_definition *const new_definition = definitions->elements + definition_index;
        new_definition->definition.data = definition_buffer.data;
        new_definition->definition.length = definition_buffer.used;
        new_definition->order = definitions->next_order++;
        return stream_writer_write_unicode_view(c_output, unicode_view_from_string(name));
    }
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
        return success_yes;

    case register_meaning_unit:
        state->standard_library.using_unit = true;
        return stream_writer_write_string(c_output, "unit_impl");
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_add_reference_to_tuple(unicode_view const tuple_name, type const *const elements,
                                                         size_t const element_count, size_t const indentation,
                                                         checked_program const *const program,
                                                         stream_writer const c_output)
{
    for (struct_member_id i = 0; i < element_count; ++i)
    {
        memory_writer name_buffer = {NULL, 0, 0};
        LPG_TRY(stream_writer_write_unicode_view(memory_writer_erase(&name_buffer), tuple_name));
        LPG_TRY(stream_writer_write_string(memory_writer_erase(&name_buffer), "."));
        LPG_TRY(generate_tuple_element_name(i, memory_writer_erase(&name_buffer)));
        LPG_TRY(
            generate_add_reference(memory_writer_content(name_buffer), elements[i], indentation, program, c_output));
        memory_writer_free(&name_buffer);
    }
    return success_yes;
}

static success_indicator generate_add_reference_to_structure(unicode_view const structure_name, structure const type_of,
                                                             size_t const indentation,
                                                             checked_program const *const program,
                                                             stream_writer const c_output)
{
    for (struct_member_id i = 0; i < type_of.count; ++i)
    {
        memory_writer name_buffer = {NULL, 0, 0};
        LPG_TRY(stream_writer_write_unicode_view(memory_writer_erase(&name_buffer), structure_name));
        LPG_TRY(stream_writer_write_string(memory_writer_erase(&name_buffer), "."));
        LPG_TRY(generate_struct_member_name(
            unicode_view_from_string(type_of.members[i].name), memory_writer_erase(&name_buffer)));
        LPG_TRY(generate_add_reference(
            memory_writer_content(name_buffer), type_of.members[i].what, indentation, program, c_output));
        memory_writer_free(&name_buffer);
    }
    return success_yes;
}

static success_indicator generate_add_reference(unicode_view const pointer_name, type const what,
                                                size_t const indentation, checked_program const *const program,
                                                stream_writer const c_output)
{
    switch (what.kind)
    {
    case type_kind_string_ref:
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "string_ref_add_reference(&"));
        LPG_TRY(stream_writer_write_unicode_view(c_output, pointer_name));
        return stream_writer_write_string(c_output, ");\n");

    case type_kind_enumeration:
    case type_kind_unit:
    case type_kind_integer_range:
    case type_kind_type:
    case type_kind_function_pointer:
    case type_kind_generic_interface:
    case type_kind_generic_enum:
        return success_yes;

    case type_kind_tuple:
        return generate_add_reference_to_tuple(
            pointer_name, what.tuple_.elements, what.tuple_.length, indentation, program, c_output);

    case type_kind_lambda:
    {
        tuple_type const captures = program->functions[what.lambda.lambda].signature->captures;
        return generate_add_reference_to_tuple(
            pointer_name, captures.elements, captures.length, indentation, program, c_output);
    }

    case type_kind_interface:
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(stream_writer_write_unicode_view(c_output, pointer_name));
        LPG_TRY(stream_writer_write_string(c_output, ".vtable->_add_reference("));
        LPG_TRY(stream_writer_write_unicode_view(c_output, pointer_name));
        LPG_TRY(stream_writer_write_string(c_output, ".self, 1);\n"));
        return success_yes;

    case type_kind_structure:
    {
        return generate_add_reference_to_structure(
            pointer_name, program->structs[what.structure_], indentation, program, c_output);
    }

    case type_kind_method_pointer:
    case type_kind_enum_constructor:
        LPG_TO_DO();
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_add_reference_to_register(checked_function const *const current_function,
                                                            register_id const where, type const what,
                                                            size_t const indentation,
                                                            checked_program const *const program,
                                                            stream_writer const c_output)
{
    memory_writer name_buffer = {NULL, 0, 0};
    LPG_TRY(generate_register_name(where, current_function, memory_writer_erase(&name_buffer)));
    success_indicator const result =
        generate_add_reference(memory_writer_content(name_buffer), what, indentation, program, c_output);
    memory_writer_free(&name_buffer);
    return result;
}

static success_indicator generate_add_reference_for_return_value(c_backend_state *state,
                                                                 checked_function const *const current_function,
                                                                 register_id const from, size_t const indentation,
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
            return success_yes;

        case register_resource_ownership_borrows:
            ASSUME(state->registers[from].type_of.is_set);
            return generate_add_reference_to_register(
                current_function, from, state->registers[from].type_of.value, indentation, state->program, c_output);
        }

    case register_meaning_assert:
    case register_meaning_string_equals:
    case register_meaning_integer_equals:
    case register_meaning_concat:
    case register_meaning_side_effect:
    case register_meaning_not:
    case register_meaning_integer_less:
    case register_meaning_integer_to_string:
    case register_meaning_unit:
        return success_yes;

    case register_meaning_nothing:
    case register_meaning_captures:
    case register_meaning_global:
        LPG_UNREACHABLE();
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_sequence(c_backend_state *state, checked_function const *const current_function,
                                           instruction_sequence const sequence, size_t const indentation,
                                           stream_writer const c_output);

static type find_boolean(void)
{
    return type_from_enumeration(0);
}

static success_indicator generate_tuple_initializer_from_registers(c_backend_state *state,
                                                                   checked_function const *const current_function,
                                                                   size_t const element_count,
                                                                   register_id *const elements,
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
    return success_yes;
}

static success_indicator generate_value(value const generated, type const type_of, c_backend_state *state,
                                        stream_writer const c_output);

static success_indicator generate_structure_initializer_from_values(c_backend_state *state, size_t const element_count,
                                                                    value const *const elements,
                                                                    structure_member const *const members,
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
            LPG_TRY(generate_value(elements[i], members[i].what, state, c_output));
        }
    }
    else
    {
        /*for dummy in struct unit*/
        LPG_TRY(stream_writer_write_string(c_output, "0"));
    }
    LPG_TRY(stream_writer_write_string(c_output, "}"));
    return success_yes;
}

static success_indicator generate_structure_variable(c_backend_state *state,
                                                     checked_function const *const current_function,
                                                     struct_id const root_id, structure_value const elements,
                                                     register_id const result, stream_writer const c_output)
{
    set_register_variable(state, result, register_resource_ownership_owns, type_from_struct(root_id));
    structure const root = state->program->structs[root_id];
    LPG_TRY(generate_type(
        type_from_struct(root_id), &state->standard_library, state->definitions, state->program, c_output));
    LPG_TRY(stream_writer_write_string(c_output, " const "));
    LPG_TRY(generate_register_name(result, current_function, c_output));
    LPG_TRY(stream_writer_write_string(c_output, " = "));
    ASSUME(elements.count == root.count);
    LPG_TRY(generate_structure_initializer_from_values(state, root.count, elements.members, root.members, c_output));
    LPG_TRY(stream_writer_write_string(c_output, ";\n"));
    return success_yes;
}

static success_indicator generate_tuple_variable(c_backend_state *state, checked_function const *const current_function,
                                                 tuple_type const tuple, register_id *const elements,
                                                 register_id const result, size_t const indentation,
                                                 stream_writer const c_output)
{
    set_register_variable(state, result, register_resource_ownership_owns, type_from_tuple_type(tuple));
    for (size_t i = 0; i < tuple.length; ++i)
    {
        LPG_TRY(generate_add_reference_to_register(
            current_function, elements[i], tuple.elements[i], indentation, state->program, c_output));
    }
    LPG_TRY(indent(indentation, c_output));
    LPG_TRY(generate_type(
        type_from_tuple_type(tuple), &state->standard_library, state->definitions, state->program, c_output));
    LPG_TRY(stream_writer_write_string(c_output, " const "));
    LPG_TRY(generate_register_name(result, current_function, c_output));
    LPG_TRY(stream_writer_write_string(c_output, " = "));
    LPG_TRY(generate_tuple_initializer_from_registers(state, current_function, tuple.length, elements, c_output));
    LPG_TRY(stream_writer_write_string(c_output, ";\n"));
    return success_yes;
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
                                                   indentation, state->program, c_output));
    }
    LPG_TRY(indent(indentation, c_output));
    LPG_TRY(generate_type(type_from_struct(generated.instantiated), &state->standard_library, state->definitions,
                          state->program, c_output));
    LPG_TRY(stream_writer_write_string(c_output, " const "));
    LPG_TRY(generate_register_name(generated.into, current_function, c_output));
    LPG_TRY(stream_writer_write_string(c_output, " = "));
    LPG_TRY(generate_tuple_initializer_from_registers(
        state, current_function, generated.argument_count, generated.arguments, c_output));
    LPG_TRY(stream_writer_write_string(c_output, ";\n"));
    return success_yes;
}

static success_indicator generate_erase_type(c_backend_state *state, register_id const destination,
                                             implementation_ref const impl, unicode_view const self,
                                             bool const add_reference_to_self,
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
    type const self_type = state->program->interfaces[impl.target].implementations[impl.implementation_index].self;
    LPG_TRY(generate_type(self_type, &state->standard_library, state->definitions, state->program, c_output));
    LPG_TRY(stream_writer_write_string(c_output, " *)"));
    LPG_TRY(generate_register_name(destination, current_function, c_output));
    LPG_TRY(stream_writer_write_string(c_output, ".self = "));
    LPG_TRY(stream_writer_write_unicode_view(c_output, self));
    LPG_TRY(stream_writer_write_string(c_output, ";\n"));

    if (add_reference_to_self)
    {
        LPG_TRY(generate_add_reference(self, self_type, indentation, state->program, c_output));
    }
    return success_yes;
}

static success_indicator generate_value(value const generated, type const type_of, c_backend_state *state,
                                        stream_writer const c_output)
{
    switch (generated.kind)
    {
    case value_kind_array:
        LPG_TO_DO();

    case value_kind_integer:
    {
        if (integer_less(generated.integer_, integer_create(1, 0)))
        {
            char buffer[40];
            char *formatted = integer_format(generated.integer_, lower_case_digits, 10, buffer, sizeof(buffer));
            LPG_TRY(stream_writer_write_bytes(c_output, formatted, (size_t)((buffer + sizeof(buffer)) - formatted)));
            return success_yes;
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
        return success_yes;

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
                type_from_tuple_type(state->program->functions[type_of.lambda.lambda].signature->captures), state,
                c_output));
        }
        else
        {
            LPG_TRY(generate_function_name(generated.function_pointer.code, c_output));
        }
        return success_yes;
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
        return success_yes;

    case value_kind_structure:
    {
        ASSUME(type_of.kind == type_kind_structure);
        structure const schema = state->program->structs[type_of.structure_];
        ASSUME(generated.structure.count == schema.count);
        LPG_TRY(stream_writer_write_string(c_output, "{"));
        for (size_t i = 0; i < schema.count; ++i)
        {
            if (i > 0)
            {
                LPG_TRY(stream_writer_write_string(c_output, ", "));
            }
            LPG_TRY(generate_value(generated.structure.members[i], schema.members[i].what, state, c_output));
        }
        LPG_TRY(stream_writer_write_string(c_output, "}"));
        return success_yes;
    }

    case value_kind_type_erased:
    case value_kind_pattern:
        LPG_TO_DO();

    case value_kind_type:
    case value_kind_generic_interface:
    case value_kind_generic_enum:
        state->standard_library.using_unit = true;
        LPG_TRY(stream_writer_write_string(c_output, "unit_impl"));
        return success_yes;

    case value_kind_enum_constructor:
        LPG_TRY(stream_writer_write_string(c_output, "/*enum constructor*/"));
        return success_yes;

    case value_kind_enum_element:
    {
        ASSUME(type_of.kind == type_kind_enumeration);
        enumeration const enum_ = state->program->enums[type_of.enum_];
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
            return success_yes;
        }
        char buffer[64];
        char const *const formatted = integer_format(
            integer_create(0, generated.enum_element.which), lower_case_digits, 10, buffer, sizeof(buffer));
        LPG_TRY(stream_writer_write_bytes(c_output, formatted, (size_t)((buffer + sizeof(buffer)) - formatted)));
        return success_yes;
    }

    case value_kind_unit:
        state->standard_library.using_unit = true;
        LPG_TRY(stream_writer_write_string(c_output, "unit_impl"));
        return success_yes;
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
        success_indicator const result =
            generate_free(&state->standard_library, memory_writer_content(name_buffer),
                          state->registers[which].type_of.value, state->program, indentation, c_output);
        memory_writer_free(&name_buffer);
        return result;
    }

    case register_resource_ownership_borrows:
        return success_yes;
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
    return success_yes;
}

static success_indicator generate_new_array(c_backend_state *const state,
                                            checked_function const *const current_function,
                                            new_array_instruction const new_array, size_t const indentation,
                                            stream_writer const c_output)
{
    set_register_variable(
        state, new_array.into, register_resource_ownership_owns, type_from_interface(new_array.result_type));
    LPG_TRY(indent(indentation, c_output));
    LPG_TRY(generate_interface_reference_name(new_array.result_type, c_output));
    LPG_TRY(stream_writer_write_string(c_output, " const "));
    LPG_TRY(generate_register_name(new_array.into, current_function, c_output));
    LPG_TRY(stream_writer_write_string(c_output, " = {&"));
    require_array_vtable(state->array_vtables, new_array.result_type, new_array.element_type, &state->standard_library,
                         state->definitions, state->program);
    LPG_TRY(generate_array_vtable_name(c_output, new_array.result_type));
    state->standard_library.using_stdlib = true;
    LPG_TRY(stream_writer_write_string(c_output, ", malloc(sizeof("));
    LPG_TRY(generate_array_impl_name(c_output, new_array.result_type));
    LPG_TRY(stream_writer_write_string(c_output, "))};\n"));

    LPG_TRY(indent(indentation, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "(("));
    LPG_TRY(generate_array_impl_name(c_output, new_array.result_type));
    LPG_TRY(stream_writer_write_string(c_output, " *)"));
    LPG_TRY(generate_register_name(new_array.into, current_function, c_output));
    LPG_TRY(stream_writer_write_string(c_output, ".self)->elements = NULL;\n"));

    LPG_TRY(indent(indentation, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "(("));
    LPG_TRY(generate_array_impl_name(c_output, new_array.result_type));
    LPG_TRY(stream_writer_write_string(c_output, " *)"));
    LPG_TRY(generate_register_name(new_array.into, current_function, c_output));
    LPG_TRY(stream_writer_write_string(c_output, ".self)->used = 0;\n"));

    LPG_TRY(indent(indentation, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "(("));
    LPG_TRY(generate_array_impl_name(c_output, new_array.result_type));
    LPG_TRY(stream_writer_write_string(c_output, " *)"));
    LPG_TRY(generate_register_name(new_array.into, current_function, c_output));
    LPG_TRY(stream_writer_write_string(c_output, ".self)->allocated = 0;\n"));

    LPG_TRY(indent(indentation, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "(("));
    LPG_TRY(generate_array_impl_name(c_output, new_array.result_type));
    LPG_TRY(stream_writer_write_string(c_output, " *)"));
    LPG_TRY(generate_register_name(new_array.into, current_function, c_output));
    LPG_TRY(stream_writer_write_string(c_output, ".self)->references = 1;\n"));
    return success_yes;
}

static success_indicator generate_instruction(c_backend_state *state, checked_function const *const current_function,
                                              instruction const input, size_t const indentation,
                                              stream_writer const c_output)
{
    switch (input.type)
    {
    case instruction_new_array:
        return generate_new_array(state, current_function, input.new_array, indentation, c_output);

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
                                    current_function, indentation, c_output));
        memory_writer_free(&original_self);
        return success_yes;
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
        return success_yes;
    }

    case instruction_return:
        set_register_variable(state, input.return_.unit_goes_into, register_resource_ownership_owns, type_from_unit());
        LPG_TRY(
            generate_free_registers(state, 0, indentation, current_function, input.return_.returned_value, c_output));
        LPG_TRY(generate_add_reference_for_return_value(
            state, current_function, input.return_.returned_value, indentation, c_output));
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "return "));
        LPG_TRY(generate_c_read_access(state, current_function, input.return_.returned_value, c_output));
        LPG_TRY(stream_writer_write_string(c_output, ";\n"));
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "unit const "));
        LPG_TRY(generate_register_name(input.return_.unit_goes_into, current_function, c_output));
        LPG_TRY(stream_writer_write_string(c_output, " = unit_impl;\n"));
        return success_yes;

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
            return success_yes;

        case register_meaning_assert:
            state->standard_library.using_assert = true;
            set_register_variable(state, input.call.result, register_resource_ownership_owns, type_from_unit());
            LPG_TRY(stream_writer_write_string(c_output, "unit const "));
            LPG_TRY(generate_register_name(input.call.result, current_function, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = assert_impl("));
            ASSERT(input.call.argument_count == 1);
            LPG_TRY(generate_c_read_access(state, current_function, input.call.arguments[0], c_output));
            LPG_TRY(stream_writer_write_string(c_output, ");\n"));
            return success_yes;

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
            return success_yes;

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
            return success_yes;

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
            return success_yes;

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
            return success_yes;

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
            return success_yes;

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
                method_description const called_method =
                    state->program->interfaces[callee_type.method_pointer.interface_]
                        .methods[callee_type.method_pointer.method_index];
                set_register_function_variable(
                    state, input.call.result, register_resource_ownership_owns, called_method.result);
                LPG_TRY(generate_type(
                    called_method.result, &state->standard_library, state->definitions, state->program, c_output));
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
                return success_yes;
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
            case type_kind_generic_enum:
            case type_kind_generic_interface:
                LPG_TO_DO();

            case type_kind_lambda:
            {
                ASSUME(state->registers[input.call.callee].type_of.is_set);
                function_pointer const callee_signature =
                    *state->program->functions[state->registers[input.call.callee].type_of.value.lambda.lambda]
                         .signature;
                result_type = callee_signature.result;
                set_register_function_variable(state, input.call.result, register_resource_ownership_owns, result_type);
                LPG_TRY(
                    generate_type(result_type, &state->standard_library, state->definitions, state->program, c_output));
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
                return success_yes;
            }
            }
            set_register_function_variable(state, input.call.result, register_resource_ownership_owns, result_type);
            LPG_TRY(generate_type(result_type, &state->standard_library, state->definitions, state->program, c_output));
            break;
        }

        case register_meaning_not:
            ASSUME(input.call.argument_count == 1);
            set_register_variable(state, input.call.result, register_resource_ownership_owns, find_boolean());
            LPG_TRY(stream_writer_write_string(c_output, "size_t const "));
            LPG_TRY(generate_register_name(input.call.result, current_function, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = !"));
            LPG_TRY(generate_c_read_access(state, current_function, input.call.arguments[0], c_output));
            LPG_TRY(stream_writer_write_string(c_output, ";\n"));
            return success_yes;

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
        return success_yes;

    case instruction_loop:
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "for (;;)\n"));
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "{\n"));
        LPG_TRY(generate_sequence(state, current_function, input.loop.body, indentation + 1, c_output));
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "}\n"));
        state->standard_library.using_unit = true;
        set_register_variable(state, input.loop.unit_goes_into, register_resource_ownership_borrows, type_from_unit());
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "unit const "));
        LPG_TRY(generate_register_name(input.loop.unit_goes_into, current_function, c_output));
        LPG_TRY(stream_writer_write_string(c_output, " = unit_impl;\n"));
        return success_yes;

    case instruction_global:
        set_register_meaning(
            state, input.global_into, optional_type_create_set(type_from_struct(0)), register_meaning_global);
        return success_yes;

    case instruction_read_struct:
        switch (state->registers[input.read_struct.from_object].meaning)
        {
        case register_meaning_nothing:
        case register_meaning_not:
        case register_meaning_unit:
            LPG_UNREACHABLE();

        case register_meaning_capture:
            LPG_TO_DO();

        case register_meaning_global:
            switch (input.read_struct.member)
            {
            case 0:
                set_register_meaning(
                    state, input.read_struct.into, optional_type_create_empty(), register_meaning_side_effect);
                return success_yes;

            case 1:
                set_register_meaning(
                    state, input.read_struct.into, optional_type_create_empty(), register_meaning_integer_to_string);
                return success_yes;

            case 4:
                set_register_meaning(
                    state, input.read_struct.into, optional_type_create_empty(), register_meaning_assert);
                return success_yes;

            case 5:
                set_register_meaning(
                    state, input.read_struct.into, optional_type_create_empty(), register_meaning_integer_less);
                return success_yes;

            case 6:
                set_register_meaning(
                    state, input.read_struct.into, optional_type_create_empty(), register_meaning_integer_equals);
                return success_yes;

            case 7:
                set_register_meaning(state, input.read_struct.into, optional_type_create_empty(), register_meaning_not);
                return success_yes;

            case 8:
                set_register_meaning(
                    state, input.read_struct.into, optional_type_create_empty(), register_meaning_concat);
                return success_yes;

            case 9:
                set_register_meaning(
                    state, input.read_struct.into, optional_type_create_empty(), register_meaning_string_equals);
                return success_yes;

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
                structure const struct_ = state->program->structs[object_type.structure_];
                structure_member const member = struct_.members[input.read_struct.member];
                set_register_variable(state, input.read_struct.into, register_resource_ownership_borrows, member.what);
                LPG_TRY(indent(indentation, c_output));
                LPG_TRY(
                    generate_type(member.what, &state->standard_library, state->definitions, state->program, c_output));
                LPG_TRY(stream_writer_write_string(c_output, " const "));
                LPG_TRY(generate_register_name(input.read_struct.into, current_function, c_output));
                LPG_TRY(stream_writer_write_string(c_output, " = "));
                LPG_TRY(generate_register_name(input.read_struct.from_object, current_function, c_output));
                LPG_TRY(stream_writer_write_string(c_output, "."));
                LPG_TRY(generate_struct_member_name(unicode_view_from_string(member.name), c_output));
                LPG_TRY(stream_writer_write_string(c_output, ";\n"));
                return success_yes;
            }

            case type_kind_tuple:
            {
                type const element_type = object_type.tuple_.elements[input.read_struct.member];
                set_register_variable(state, input.read_struct.into, register_resource_ownership_borrows, element_type);
                LPG_TRY(indent(indentation, c_output));
                LPG_TRY(generate_type(
                    element_type, &state->standard_library, state->definitions, state->program, c_output));
                LPG_TRY(stream_writer_write_string(c_output, " const "));
                LPG_TRY(generate_register_name(input.read_struct.into, current_function, c_output));
                LPG_TRY(stream_writer_write_string(c_output, " = "));
                LPG_TRY(generate_register_name(input.read_struct.from_object, current_function, c_output));
                LPG_TRY(stream_writer_write_string(c_output, "."));
                LPG_TRY(generate_tuple_element_name(input.read_struct.member, c_output));
                LPG_TRY(stream_writer_write_string(c_output, ";\n"));
                return success_yes;
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
            case type_kind_generic_enum:
            case type_kind_generic_interface:
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
            return success_yes;
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
        return success_yes;

    case instruction_literal:
        LPG_TRY(indent(indentation, c_output));
        ASSERT(state->registers[input.literal.into].meaning == register_meaning_nothing);
        switch (input.literal.value_.kind)
        {
        case value_kind_array:
            LPG_TO_DO();

        case value_kind_generic_enum:
            set_register_variable(
                state, input.literal.into, register_resource_ownership_borrows, input.literal.type_of);
            LPG_TRY(stream_writer_write_string(c_output, "/*generic enum omitted*/\n"));
            return success_yes;

        case value_kind_generic_interface:
            set_register_variable(
                state, input.literal.into, register_resource_ownership_borrows, input.literal.type_of);
            LPG_TRY(stream_writer_write_string(c_output, "/*generic interface omitted*/\n"));
            return success_yes;

        case value_kind_integer:
            set_register_variable(
                state, input.literal.into, register_resource_ownership_borrows, input.literal.type_of);
            LPG_TRY(stream_writer_write_string(c_output, "uint64_t const "));
            LPG_TRY(generate_register_name(input.literal.into, current_function, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = "));
            LPG_TRY(generate_value(input.literal.value_, input.literal.type_of, state, c_output));
            LPG_TRY(stream_writer_write_string(c_output, ";\n"));
            return success_yes;

        case value_kind_string:
            set_register_variable(
                state, input.literal.into, register_resource_ownership_borrows, input.literal.type_of);
            state->standard_library.using_string_ref = true;
            LPG_TRY(stream_writer_write_string(c_output, "string_ref const "));
            LPG_TRY(generate_register_name(input.literal.into, current_function, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = "));
            LPG_TRY(generate_value(input.literal.value_, input.literal.type_of, state, c_output));
            LPG_TRY(stream_writer_write_string(c_output, ";\n"));
            return success_yes;

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
                    state->program->functions[input.literal.value_.function_pointer.code].signature->captures;
                LPG_TRY(generate_type(type_from_tuple_type(captures), &state->standard_library, state->definitions,
                                      state->program, c_output));
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
                        state->program->functions[input.literal.value_.function_pointer.code].signature),
                    &state->standard_library, state->definitions, state->program, c_output));
                LPG_TRY(stream_writer_write_string(c_output, " const "));
                LPG_TRY(generate_register_name(input.literal.into, current_function, c_output));
                LPG_TRY(stream_writer_write_string(c_output, " = "));
                LPG_TRY(generate_value(input.literal.value_, input.literal.type_of, state, c_output));
                LPG_TRY(stream_writer_write_string(c_output, ";\n"));
            }
            return success_yes;

        case value_kind_tuple:
            set_register_variable(
                state, input.literal.into, register_resource_ownership_borrows, input.literal.type_of);
            ASSUME(input.literal.type_of.kind == type_kind_tuple);
            ASSUME(input.literal.value_.tuple_.element_count == input.literal.type_of.tuple_.length);
            LPG_TRY(generate_type(
                input.literal.type_of, &state->standard_library, state->definitions, state->program, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " const "));
            LPG_TRY(generate_register_name(input.literal.into, current_function, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = "));
            LPG_TRY(generate_value(input.literal.value_, input.literal.type_of, state, c_output));
            LPG_TRY(stream_writer_write_string(c_output, ";\n"));
            return success_yes;

        case value_kind_structure:
            ASSUME(input.literal.type_of.kind == type_kind_structure);
            return generate_structure_variable(state, current_function, input.literal.type_of.structure_,
                                               input.literal.value_.structure, input.literal.into, c_output);

        case value_kind_type_erased:
        {
            set_register_variable(state, input.literal.into, register_resource_ownership_owns, input.literal.type_of);
            ASSUME(input.literal.type_of.kind == type_kind_interface);
            memory_writer self = {NULL, 0, 0};
            LPG_TRY(generate_value(
                *input.literal.value_.type_erased.self, input.literal.type_of, state, memory_writer_erase(&self)));
            LPG_TRY(generate_erase_type(state, input.literal.into, input.literal.value_.type_erased.impl,
                                        unicode_view_create(self.data, self.used), false, current_function, indentation,
                                        c_output));
            memory_writer_free(&self);
            return success_yes;
        }

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
            return success_yes;

        case value_kind_enum_constructor:
            set_register_variable(
                state, input.literal.into, register_resource_ownership_borrows, input.literal.type_of);
            LPG_TRY(stream_writer_write_string(c_output, "/*enum constructor omitted*/\n"));
            return success_yes;

        case value_kind_enum_element:
        {
            set_register_variable(
                state, input.literal.into, register_resource_ownership_borrows, input.literal.type_of);
            LPG_TRY(generate_type(
                input.literal.type_of, &state->standard_library, state->definitions, state->program, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " const "));
            LPG_TRY(generate_register_name(input.literal.into, current_function, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = "));
            LPG_TRY(generate_value(input.literal.value_, input.literal.type_of, state, c_output));
            LPG_TRY(stream_writer_write_string(c_output, ";\n"));
            return success_yes;
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
            return success_yes;
        }
        LPG_UNREACHABLE();

    case instruction_tuple:
        ASSUME(input.type == instruction_tuple);
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
                              state->definitions, state->program, c_output));
        LPG_TRY(stream_writer_write_string(c_output, " const "));
        LPG_TRY(generate_register_name(input.enum_construct.into, current_function, c_output));
        LPG_TRY(stream_writer_write_string(c_output, " = {"));
        LPG_TRY(stream_writer_write_integer(c_output, integer_create(0, input.enum_construct.which.which)));
        LPG_TRY(stream_writer_write_string(c_output, ", {."));
        enumeration const enum_ = state->program->enums[input.enum_construct.which.enumeration];
        LPG_TRY(generate_struct_member_name(
            unicode_view_from_string(enum_.elements[input.enum_construct.which.which].name), c_output));
        LPG_TRY(stream_writer_write_string(c_output, " = "));
        LPG_TRY(generate_c_read_access(state, current_function, input.enum_construct.state, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "}};\n"));
        LPG_TRY(generate_add_reference_to_register(current_function, input.enum_construct.state,
                                                   enum_.elements[input.enum_construct.which.which].state, indentation,
                                                   state->program, c_output));
        return success_yes;
    }

    case instruction_match:
    {
        set_register_variable(state, input.match.result, register_resource_ownership_owns, input.match.result_type);
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(generate_type(
            input.match.result_type, &state->standard_library, state->definitions, state->program, c_output));
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
                if ((key_type.kind == type_kind_enumeration) &&
                    has_stateful_element(state->program->enums[key_type.enum_]))
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
                enumeration const enum_ = state->program->enums[state->registers[input.match.key].type_of.value.enum_];
                type const state_type = enum_.elements[input.match.cases[i].stateful_enum.element].state;
                set_register_variable(
                    state, input.match.cases[i].stateful_enum.where, register_resource_ownership_owns, state_type);
                LPG_TRY(indent(indentation + 1, c_output));
                LPG_TRY(
                    generate_type(state_type, &state->standard_library, state->definitions, state->program, c_output));
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
                                                           state_type, indentation + 1, state->program, c_output));
                ASSUME(state->active_register_count == (previous_register_count + 1));
                break;
            }

            case match_instruction_case_kind_value:
                break;
            }
            LPG_TRY(
                generate_sequence(state, current_function, input.match.cases[i].action, (indentation + 1), c_output));

            LPG_TRY(generate_free_registers(
                state, previous_register_count, indentation + 1, current_function, ~(register_id)0, c_output));

            LPG_TRY(indent(indentation + 1, c_output));
            LPG_TRY(generate_register_name(input.match.result, current_function, c_output));
            LPG_TRY(stream_writer_write_string(c_output, " = "));
            LPG_TRY(generate_c_read_access(state, current_function, input.match.cases[i].value, c_output));
            LPG_TRY(stream_writer_write_string(c_output, ";\n"));

            LPG_TRY(generate_add_reference_to_register(current_function, input.match.result, input.match.result_type,
                                                       indentation + 1, state->program, c_output));

            LPG_TRY(indent(indentation, c_output));
            LPG_TRY(stream_writer_write_string(c_output, "}\n"));
        }
        return success_yes;
    }

    case instruction_get_captures:
        set_register_meaning(state, input.captures,
                             optional_type_create_set(type_from_tuple_type(current_function->signature->captures)),
                             register_meaning_captures);
        return success_yes;

    case instruction_lambda_with_captures:
    {
        type const function_type = type_from_lambda(lambda_type_create(input.lambda_with_captures.lambda));
        set_register_variable(state, input.lambda_with_captures.into, register_resource_ownership_owns, function_type);
        tuple_type const captures = state->program->functions[input.lambda_with_captures.lambda].signature->captures;
        for (size_t i = 0; i < captures.length; ++i)
        {
            LPG_TRY(generate_add_reference_to_register(current_function, input.lambda_with_captures.captures[i],
                                                       captures.elements[i], indentation, state->program, c_output));
        }
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(generate_type(function_type, &state->standard_library, state->definitions, state->program, c_output));
        LPG_TRY(stream_writer_write_string(c_output, " const "));
        LPG_TRY(generate_register_name(input.lambda_with_captures.into, current_function, c_output));
        LPG_TRY(stream_writer_write_string(c_output, " = "));
        LPG_TRY(generate_tuple_initializer_from_registers(
            state, current_function, captures.length, input.lambda_with_captures.captures, c_output));
        LPG_TRY(stream_writer_write_string(c_output, ";\n"));
        return success_yes;
    }
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_free_enumeration(standard_library_usage *const standard_library,
                                                   unicode_view const freed, enumeration const what,
                                                   checked_program const *const program, size_t const indentation,
                                                   stream_writer const c_output)
{
    if (!has_stateful_element(what))
    {
        return success_yes;
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
        LPG_TRY(generate_free(standard_library, memory_writer_content(name_buffer), what.elements[i].state, program,
                              indentation + 1, c_output));
        memory_writer_free(&name_buffer);
        LPG_TRY(indent(indentation + 1, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "break;\n"));
    }
    LPG_TRY(indent(indentation, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "}\n"));
    return success_yes;
}

static success_indicator generate_free(standard_library_usage *const standard_library, unicode_view const freed,
                                       type const what, checked_program const *const program, size_t const indentation,
                                       stream_writer const c_output)
{
    switch (what.kind)
    {
    case type_kind_method_pointer:
        return generate_free(standard_library, freed, type_from_interface(what.method_pointer.interface_), program,
                             indentation, c_output);

    case type_kind_interface:
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(stream_writer_write_unicode_view(c_output, freed));
        LPG_TRY(stream_writer_write_string(c_output, ".vtable->_add_reference("));
        LPG_TRY(stream_writer_write_unicode_view(c_output, freed));
        LPG_TRY(stream_writer_write_string(c_output, ".self, -1);\n"));
        return success_yes;

    case type_kind_string_ref:
        standard_library->using_string_ref = true;
        LPG_TRY(indent(indentation, c_output));
        LPG_TRY(stream_writer_write_string(c_output, "string_ref_free(&"));
        LPG_TRY(stream_writer_write_unicode_view(c_output, freed));
        LPG_TRY(stream_writer_write_string(c_output, ");\n"));
        return success_yes;

    case type_kind_structure:
    {
        structure const struct_ = program->structs[what.structure_];
        for (struct_member_id i = 0; i < struct_.count; ++i)
        {
            memory_writer name_buffer = {NULL, 0, 0};
            LPG_TRY(stream_writer_write_unicode_view(memory_writer_erase(&name_buffer), freed));
            LPG_TRY(stream_writer_write_string(memory_writer_erase(&name_buffer), "."));
            LPG_TRY(generate_struct_member_name(
                unicode_view_from_string(struct_.members[i].name), memory_writer_erase(&name_buffer)));
            LPG_TRY(generate_free(standard_library, memory_writer_content(name_buffer), struct_.members[i].what,
                                  program, indentation, c_output));
            memory_writer_free(&name_buffer);
        }
        return success_yes;
    }

    case type_kind_tuple:
        for (struct_member_id i = 0; i < what.tuple_.length; ++i)
        {
            memory_writer name_buffer = {NULL, 0, 0};
            LPG_TRY(stream_writer_write_unicode_view(memory_writer_erase(&name_buffer), freed));
            LPG_TRY(stream_writer_write_string(memory_writer_erase(&name_buffer), "."));
            LPG_TRY(generate_tuple_element_name(i, memory_writer_erase(&name_buffer)));
            LPG_TRY(generate_free(standard_library, memory_writer_content(name_buffer), what.tuple_.elements[i],
                                  program, indentation, c_output));
            memory_writer_free(&name_buffer);
        }
        return success_yes;

    case type_kind_enum_constructor:
    case type_kind_function_pointer:
    case type_kind_type:
    case type_kind_unit:
    case type_kind_integer_range:
    case type_kind_generic_enum:
    case type_kind_generic_interface:
        return success_yes;

    case type_kind_enumeration:
        return generate_free_enumeration(
            standard_library, freed, program->enums[what.enum_], program, indentation, c_output);

    case type_kind_lambda:
    {
        checked_function const lambda_function = program->functions[what.lambda.lambda];
        LPG_TRY(generate_free(standard_library, freed, type_from_tuple_type(lambda_function.signature->captures),
                              program, indentation, c_output));
        return success_yes;
    }
    }
    LPG_UNREACHABLE();
}

static success_indicator generate_sequence(c_backend_state *state, checked_function const *const current_function,
                                           instruction_sequence const sequence, size_t const indentation,
                                           stream_writer const c_output)
{
    size_t const previously_active_registers = state->active_register_count;
    for (size_t i = 0; i < sequence.length; ++i)
    {
        LPG_TRY(generate_instruction(state, current_function, sequence.elements[i], indentation, c_output));
    }
    return generate_free_registers(
        state, previously_active_registers, indentation, current_function, ~(register_id)0, c_output);
}

static success_indicator generate_function_body(checked_function const current_function,
                                                checked_program const *const program, stream_writer const c_output,
                                                standard_library_usage *standard_library,
                                                type_definitions *const definitions,
                                                array_vtable_cache *const array_vtables)
{
    LPG_TRY(stream_writer_write_string(c_output, "{\n"));

    c_backend_state state = {allocate_array(current_function.number_of_registers, sizeof(*state.registers)),
                             *standard_library,
                             NULL,
                             0,
                             definitions,
                             program,
                             array_vtables};
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
        LPG_TRY(
            generate_type(current_function.signature->self.value, standard_library, definitions, program, c_output));
        LPG_TRY(stream_writer_write_string(c_output, " const "));
        LPG_TRY(generate_register_name(next_free_register, &current_function, c_output));
        LPG_TRY(stream_writer_write_string(c_output, " = *("));
        LPG_TRY(
            generate_type(current_function.signature->self.value, standard_library, definitions, program, c_output));
        LPG_TRY(stream_writer_write_string(c_output, " const *)self;\n"));
        ++next_free_register;
    }

    for (register_id i = 0; i < current_function.signature->parameters.length; ++i)
    {
        type const parameter = current_function.signature->parameters.elements[i];
        set_register_argument(&state, next_free_register, register_resource_ownership_borrows, parameter);
        ++next_free_register;
    }
    LPG_TRY(generate_sequence(&state, &current_function, current_function.body, 1, c_output));
    LPG_TRY(stream_writer_write_string(c_output, "}\n"));
    *standard_library = state.standard_library;
    deallocate(state.registers);
    if (state.active_registers)
    {
        deallocate(state.active_registers);
    }
    return success_yes;
}

static success_indicator generate_function_declaration(function_id const id, function_pointer const signature,
                                                       standard_library_usage *const standard_library,
                                                       type_definitions *const definitions,
                                                       checked_program const *const program,
                                                       stream_writer const program_defined_writer)
{
    LPG_TRY(stream_writer_write_string(program_defined_writer, "static "));
    LPG_TRY(generate_type(signature.result, standard_library, definitions, program, program_defined_writer));
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
        LPG_TRY(generate_type(
            type_from_tuple_type(signature.captures), standard_library, definitions, program, program_defined_writer));
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
        LPG_TRY(generate_type(
            signature.parameters.elements[j], standard_library, definitions, program, program_defined_writer));
        LPG_TRY(stream_writer_write_string(program_defined_writer, " const "));
        LPG_TRY(generate_register_name(signature.self.is_set + j, program->functions + id, program_defined_writer));
    }
    LPG_TRY(stream_writer_write_string(program_defined_writer, ")"));
    return success_yes;
}

success_indicator generate_c(checked_program const program, stream_writer const c_output)
{
    memory_writer program_defined_a = {NULL, 0, 0};
    stream_writer const program_defined_writer_a = memory_writer_erase(&program_defined_a);
    memory_writer program_defined_b = {NULL, 0, 0};
    stream_writer const program_defined_writer_b = memory_writer_erase(&program_defined_b);

    standard_library_usage standard_library = {false, false, false, false, false, false, false, false};

    type_definitions definitions = {NULL, 0, 0};

    for (interface_id i = 0; i < program.interface_count; ++i)
    {
        LPG_TRY_GOTO(generate_interface_vtable_definition(
                         i, &standard_library, &definitions, &program, program_defined_writer_a),
                     fail);
    }

    for (enum_id i = 0; i < program.enum_count; ++i)
    {
        if (!has_stateful_element(program.enums[i]))
        {
            continue;
        }
        LPG_TRY_GOTO(
            generate_stateful_enum_definition(i, &standard_library, &definitions, &program, program_defined_writer_a),
            fail);
    }

    for (function_id i = 1; i < program.function_count; ++i)
    {
        checked_function const current_function = program.functions[i];
        LPG_TRY_GOTO(generate_function_declaration(i, *current_function.signature, &standard_library, &definitions,
                                                   &program, program_defined_writer_a),
                     fail);
        LPG_TRY_GOTO(stream_writer_write_string(program_defined_writer_a, ";\n"), fail);
    }

    for (interface_id i = 0; i < program.interface_count; ++i)
    {
        lpg_interface const interface_ = program.interfaces[i];
        for (size_t k = 0; k < interface_.implementation_count; ++k)
        {
            LPG_TRY_GOTO(generate_interface_impl_definition(implementation_ref_create(i, k), &definitions, &program,
                                                            &standard_library, program_defined_writer_a),
                         fail);
        }
    }

    array_vtable_cache array_vtables = {NULL, 0};

    for (function_id i = 1; i < program.function_count; ++i)
    {
        checked_function const current_function = program.functions[i];
        LPG_TRY_GOTO(generate_function_declaration(i, *current_function.signature, &standard_library, &definitions,
                                                   &program, program_defined_writer_b),
                     fail);
        LPG_TRY_GOTO(stream_writer_write_string(program_defined_writer_b, "\n"), fail);
        LPG_TRY_GOTO(generate_function_body(current_function, &program, program_defined_writer_b, &standard_library,
                                            &definitions, &array_vtables),
                     fail);
    }

    LPG_TRY_GOTO(stream_writer_write_string(program_defined_writer_b, "static "), fail);
    LPG_TRY_GOTO(generate_type(program.functions[0].signature->result, &standard_library, &definitions, &program,
                               program_defined_writer_b),
                 fail);
    LPG_TRY_GOTO(stream_writer_write_string(program_defined_writer_b, " lpg_main(void)\n"), fail);
    LPG_TRY_GOTO(generate_function_body(program.functions[0], &program, program_defined_writer_b, &standard_library,
                                        &definitions, &array_vtables),
                 fail);
    LPG_TRY_GOTO(stream_writer_write_string(program_defined_writer_b, "int main(void)\n"), fail);
    LPG_TRY_GOTO(stream_writer_write_string(program_defined_writer_b, "{\n"), fail);
    LPG_TRY_GOTO(indent(1, program_defined_writer_b), fail);
    LPG_TRY_GOTO(generate_type(program.functions[0].signature->result, &standard_library, &definitions, &program,
                               program_defined_writer_b),
                 fail);
    LPG_TRY_GOTO(stream_writer_write_string(program_defined_writer_b, " const result = lpg_main();\n"), fail);
    LPG_TRY_GOTO(generate_free(&standard_library, unicode_view_from_c_str("result"),
                               program.functions[0].signature->result, &program, 1, program_defined_writer_b),
                 fail);
    LPG_TRY_GOTO(indent(1, program_defined_writer_b), fail);
    LPG_TRY_GOTO(stream_writer_write_string(program_defined_writer_b, "return 0;\n"), fail);
    LPG_TRY_GOTO(stream_writer_write_string(program_defined_writer_b, "}\n"), fail);

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
    if (standard_library.using_stdlib)
    {
        LPG_TRY_GOTO(stream_writer_write_string(c_output, "#include <stdlib.h>\n"), fail);
    }
    if (standard_library.using_integer)
    {
        LPG_TRY_GOTO(stream_writer_write_string(c_output, "#include <lpg_std_integer.h>\n"), fail);
    }
    if (standard_library.using_boolean)
    {
        LPG_TRY_GOTO(stream_writer_write_string(c_output, "#include <lpg_std_boolean.h>\n"), fail);
    }
    if (standard_library.using_c_assert)
    {
        LPG_TRY_GOTO(stream_writer_write_string(c_output, "#include <assert.h>\n"), fail);
    }
    LPG_TRY_GOTO(stream_writer_write_string(c_output, "#include <stddef.h>\n"
                                                      "typedef size_t stateless_enum;\n"),
                 fail);

    for (interface_id i = 0; i < program.interface_count; ++i)
    {
        LPG_TRY_GOTO(generate_interface_reference_definition(i, c_output), fail);
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

    for (size_t i = 0; i < array_vtables.array_vtable_count; ++i)
    {
        LPG_TRY_GOTO(stream_writer_write_unicode_view(
                         program_defined_writer_a, unicode_view_from_string(array_vtables.array_vtables[i].definition)),
                     fail);
    }
    array_vtable_cache_free(array_vtables);

    LPG_TRY_GOTO(stream_writer_write_bytes(c_output, program_defined_a.data, program_defined_a.used), fail);
    LPG_TRY_GOTO(stream_writer_write_bytes(c_output, program_defined_b.data, program_defined_b.used), fail);
    type_definitions_free(definitions);
    memory_writer_free(&program_defined_a);
    memory_writer_free(&program_defined_b);
    return success_yes;

fail:
    type_definitions_free(definitions);
    memory_writer_free(&program_defined_a);
    memory_writer_free(&program_defined_b);
    return success_no;
}
