#include "test_instruction.h"
#include "lpg_instruction.h"
#include "test.h"

void test_instruction(void)
{
    REQUIRE(literal_instruction_equals(literal_instruction_create(0, value_from_unit(), type_from_unit()),
                                       literal_instruction_create(0, value_from_unit(), type_from_unit())));
    REQUIRE(enum_construct_instruction_equals(enum_construct_instruction_create(0, 0, 0, type_from_unit()),
                                              enum_construct_instruction_create(0, 0, 0, type_from_unit())));
    REQUIRE(lambda_with_captures_instruction_equals(lambda_with_captures_instruction_create(0, 0, NULL, 0),
                                                    lambda_with_captures_instruction_create(0, 0, NULL, 0)));
    REQUIRE(!lambda_with_captures_instruction_equals(lambda_with_captures_instruction_create(1, 0, NULL, 0),
                                                     lambda_with_captures_instruction_create(0, 0, NULL, 0)));
    REQUIRE(!lambda_with_captures_instruction_equals(lambda_with_captures_instruction_create(0, 1, NULL, 0),
                                                     lambda_with_captures_instruction_create(0, 0, NULL, 0)));
    {
        register_id left = 0;
        register_id right = 0;
        REQUIRE(lambda_with_captures_instruction_equals(lambda_with_captures_instruction_create(0, 0, &left, 1),
                                                        lambda_with_captures_instruction_create(0, 0, &right, 1)));
    }
    {
        register_id left = 0;
        register_id right = 1;
        REQUIRE(!lambda_with_captures_instruction_equals(lambda_with_captures_instruction_create(0, 0, &left, 1),
                                                         lambda_with_captures_instruction_create(0, 0, &right, 1)));
    }
    REQUIRE(get_method_instruction_equals(
        get_method_instruction_create(0, 0, 0, 0), get_method_instruction_create(0, 0, 0, 0)));
    REQUIRE(erase_type_instruction_equals(erase_type_instruction_create(0, 0, implementation_ref_create(0, 0)),
                                          erase_type_instruction_create(0, 0, implementation_ref_create(0, 0))));
}
