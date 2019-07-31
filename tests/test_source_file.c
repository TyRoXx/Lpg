#include "test_source_file.h"
#include "lpg_array_size.h"
#include "lpg_source_file.h"
#include "test.h"

static void test_source_file_lines_owning_scan(char const *const source, size_t const *const line_offsets,
                                               size_t const line_count)
{
    source_file_lines_owning const lines = source_file_lines_owning_scan(unicode_view_from_c_str(source));
    source_file_lines const expected = source_file_lines_create(line_offsets, line_count);
    REQUIRE(source_file_lines_equals(expected, source_file_lines_from_owning(lines)));
    source_file_lines_owning_free(lines);
}

void test_source_file(void)
{
    {
        size_t const line_offsets[] = {0};
        test_source_file_lines_owning_scan("", line_offsets, LPG_ARRAY_SIZE(line_offsets));
    }
    {
        size_t const line_offsets[] = {0, 1};
        test_source_file_lines_owning_scan("\n", line_offsets, LPG_ARRAY_SIZE(line_offsets));
    }
    {
        size_t const line_offsets[] = {0, 1, 2, 3};
        test_source_file_lines_owning_scan("\n\n\n", line_offsets, LPG_ARRAY_SIZE(line_offsets));
    }
    {
        size_t const line_offsets[] = {0, 1, 3, 5};
        test_source_file_lines_owning_scan("\na\nb\n", line_offsets, LPG_ARRAY_SIZE(line_offsets));
    }
    {
        size_t const line_offsets[] = {0, 1};
        test_source_file_lines_owning_scan("\na", line_offsets, LPG_ARRAY_SIZE(line_offsets));
    }
    {
        size_t const line_offsets[] = {0, 1};
        test_source_file_lines_owning_scan("\r", line_offsets, LPG_ARRAY_SIZE(line_offsets));
    }
    {
        size_t const line_offsets[] = {0, 1, 2, 3};
        test_source_file_lines_owning_scan("\r\r\r", line_offsets, LPG_ARRAY_SIZE(line_offsets));
    }
    {
        size_t const line_offsets[] = {0, 1, 3, 5};
        test_source_file_lines_owning_scan("\ra\rb\r", line_offsets, LPG_ARRAY_SIZE(line_offsets));
    }
    {
        size_t const line_offsets[] = {0, 1};
        test_source_file_lines_owning_scan("\ra", line_offsets, LPG_ARRAY_SIZE(line_offsets));
    }
    {
        size_t const line_offsets[] = {0, 2};
        test_source_file_lines_owning_scan("\r\n", line_offsets, LPG_ARRAY_SIZE(line_offsets));
    }
    {
        size_t const line_offsets[] = {0, 2, 4, 6};
        test_source_file_lines_owning_scan("\r\n\r\n\r\n", line_offsets, LPG_ARRAY_SIZE(line_offsets));
    }
    {
        size_t const line_offsets[] = {0, 2, 5, 8};
        test_source_file_lines_owning_scan("\r\na\r\nb\r\n", line_offsets, LPG_ARRAY_SIZE(line_offsets));
    }
    {
        size_t const line_offsets[] = {0, 2};
        test_source_file_lines_owning_scan("\r\na", line_offsets, LPG_ARRAY_SIZE(line_offsets));
    }
    {
        size_t const line_offsets[] = {0, 1, 2};
        test_source_file_lines_owning_scan("\r\r", line_offsets, LPG_ARRAY_SIZE(line_offsets));
    }
}
