#include "fuzz_target.h"
#include "lpg_array_size.h"
#include "lpg_ascii.h"
#include "lpg_check.h"
#include "lpg_cli.h"
#include "lpg_standard_library.h"
#include <assert.h>
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

void ParserTypeCheckerFuzzTarget(uint8_t const *const data, size_t const size)
{
    if (!is_ascii((char const *)data, size))
    {
        return;
    }
    cli_parser_user user = {stream_writer_create_null_writer(), false};
    unicode_view const source = unicode_view_create((char const *)data, size);
    source_file_lines_owning const lines = source_file_lines_owning_scan(source);
    expression_pool pool = expression_pool_create();
    optional_sequence const result =
        parse(user, unicode_view_from_c_str("fuzzing"), source, source_file_lines_from_owning(lines), &pool);
    if (result.has_value)
    {
        standard_library_description const standard_library = describe_standard_library();
        module_loader loader =
            module_loader_create(unicode_view_from_c_str(LPG_FUZZ_MODULE_DIRECTORY), ignore_parse_errors, &user);
        checked_program checked =
            check(result.value, standard_library.globals, ignore_semantic_errors, &loader,
                  source_file_create(unicode_view_from_c_str("fuzzing"), source, source_file_lines_from_owning(lines)),
                  unicode_view_from_c_str(LPG_FUZZ_MODULE_DIRECTORY), 100000, NULL);
        checked_program_free(&checked);
        standard_library_description_free(&standard_library);
    }
    sequence_free(&result.value);
    expression_pool_free(pool);
    source_file_lines_owning_free(lines);
}
