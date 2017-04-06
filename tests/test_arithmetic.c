#include "test_arithmetic.h"
#include "test.h"
#include "lpg_optional.h"
#include "lpg_arithmetic.h"

void test_arithmetic(void)
{
    {
        optional_size const product = size_multiply(0, 0);
        REQUIRE(product.state == optional_set);
        REQUIRE(product.value_if_set == 0);
    }
    {
        optional_size const product = size_multiply(SIZE_MAX, 0);
        REQUIRE(product.state == optional_set);
        REQUIRE(product.value_if_set == 0);
    }
    {
        optional_size const product = size_multiply(SIZE_MAX, 1);
        REQUIRE(product.state == optional_set);
        REQUIRE(product.value_if_set == SIZE_MAX);
    }
    {
        optional_size const product = size_multiply(SIZE_MAX, 2);
        REQUIRE(product.state == optional_empty);
    }
    {
        optional_size const product = size_multiply(SIZE_MAX, SIZE_MAX / 2);
        REQUIRE(product.state == optional_empty);
    }
    {
        optional_size const product = size_multiply(SIZE_MAX, SIZE_MAX);
        REQUIRE(product.state == optional_empty);
    }
}
