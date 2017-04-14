#include "test_semantics.h"
#include "test.h"
#include "lpg_check.h"

void test_semantics(void)
{
    {
        sequence root = sequence_create(NULL, 0);
        check(root);
        sequence_free(&root);
    }
}
