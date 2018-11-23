#include "test_enum_encoding.h"
#include "lpg_enum_encoding.h"
#include "test.h"

void test_enum_encoding(void)
{
    {
        memory_writer buffer = {NULL, 0, 0};
        ASSERT(success_yes == enum_construct_stateful_begin(0, memory_writer_erase(&buffer)));
        ASSERT(memory_writer_equals(buffer, "[0, "));
        memory_writer_free(&buffer);
    }
    {
        memory_writer buffer = {NULL, 0, 0};
        ASSERT(success_yes == enum_construct_stateful_end(memory_writer_erase(&buffer)));
        ASSERT(memory_writer_equals(buffer, "]"));
        memory_writer_free(&buffer);
    }
}
