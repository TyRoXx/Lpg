#include "test_create_process.h"
#include "lpg_create_process.h"
#include "test.h"

void test_create_process(void)
{
    {
        create_process_result result =
            create_process(unicode_view_from_c_str("does not exist"), NULL, 0, unicode_view_from_c_str("/"),
                           get_standard_input(), get_standard_output(), get_standard_error());
        REQUIRE(result.success == success_no);
    }
    {
        create_process_result result =
            create_process(unicode_view_from_c_str("/bin/true"), NULL, 0, unicode_view_from_c_str("does not exist"),
                           get_standard_input(), get_standard_output(), get_standard_error());
        REQUIRE(result.success == success_no);
    }
}
