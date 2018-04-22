#include "lpg_cli.h"
#include <stdio.h>
#if LPG_WITH_VLD
#include <vld.h>
#endif

static success_indicator write_file(void *const user, char const *const data, size_t const length)
{
    if (fwrite(data, 1, length, user) == length)
    {
        return success_yes;
    }
    return success_no;
}

static stream_writer make_file_writer(LPG_NON_NULL(FILE *const file))
{
    stream_writer const result = {write_file, file};
    return result;
}

int main(int const argc, char **const argv)
{
    stream_writer const diagnostics = make_file_writer(stderr);
    return run_cli(argc, argv, diagnostics, unicode_view_from_c_str(LPG_CLI_MODULE_DIRECTORY));
}
