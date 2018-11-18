#include "test_arithmetic.h"
#include "lpg_arithmetic.h"
#include "lpg_optional.h"
#include "test.h"

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
    {
        optional_size const sum = size_add(0, 0);
        REQUIRE(sum.state == optional_set);
        REQUIRE(sum.value_if_set == 0);
    }
    {
        optional_size const sum = size_add(1, 0);
        REQUIRE(sum.state == optional_set);
        REQUIRE(sum.value_if_set == 1);
    }
    {
        optional_size const sum = size_add(0, 1);
        REQUIRE(sum.state == optional_set);
        REQUIRE(sum.value_if_set == 1);
    }
    {
        optional_size const sum = size_add(SIZE_MAX, 0);
        REQUIRE(sum.state == optional_set);
        REQUIRE(sum.value_if_set == SIZE_MAX);
    }
    {
        optional_size const sum = size_add(SIZE_MAX - 1, 1);
        REQUIRE(sum.state == optional_set);
        REQUIRE(sum.value_if_set == SIZE_MAX);
    }
    {
        optional_size const sum = size_add(SIZE_MAX, 1);
        REQUIRE(sum.state == optional_empty);
    }
    {
        optional_size const sum = size_add(SIZE_MAX, SIZE_MAX);
        REQUIRE(sum.state == optional_empty);
    }
}
