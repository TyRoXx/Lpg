#include "lpg_allocate.h"
#include "lpg_array_size.h"
#include "lpg_ascii.h"
#include "lpg_check.h"
#include "lpg_cli.h"
#include "lpg_standard_library.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static void ignore_parse_errors(complete_parse_error error, callback_user user)
{
    (void)error;
    (void)user;
}

static void ignore_semantic_errors(complete_semantic_error error, void *user)
{
    (void)error;
    (void)user;
}

int LLVMFuzzerTestOneInput(uint8_t const *const data, size_t const size)
{
    if (!is_ascii((char const *)data, size))
    {
        return 0;
    }
    cli_parser_user user = {stream_writer_create_null_writer(), false};
    unicode_view const source = unicode_view_create((char const *)data, size);
    optional_sequence const result = parse(user, unicode_view_from_c_str("fuzzing"), source);
    if (result.has_value)
    {
        standard_library_description const standard_library = describe_standard_library();
        value globals_values[standard_library_element_count];
        for (size_t i = 0; i < LPG_ARRAY_SIZE(globals_values); ++i)
        {
            globals_values[i] = value_from_unit();
        }
        ASSUME(LPG_ARRAY_SIZE(globals_values) == standard_library.globals.count);
        module_loader loader =
            module_loader_create(unicode_view_from_c_str(LPG_FUZZ_MODULE_DIRECTORY), ignore_parse_errors, &user);
        checked_program checked = check(result.value, standard_library.globals, ignore_semantic_errors, &loader,
                                        source_file_create(unicode_view_from_c_str("fuzzing"), source),
                                        unicode_view_from_c_str(LPG_FUZZ_MODULE_DIRECTORY), NULL);
        checked_program_free(&checked);
        sequence_free(&result.value);
        standard_library_description_free(&standard_library);
    }
    ASSERT(count_active_allocations() == 0);
    return 0;
}

#if !LPG_WITH_CLANG_FUZZER
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    char const *const input = "let a = 1";
    LLVMFuzzerTestOneInput((uint8_t const *)input, strlen(input));
    return 0;
}
#endif
