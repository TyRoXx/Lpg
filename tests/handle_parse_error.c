#include "handle_parse_error.h"
#include "test.h"

void handle_error(parse_error const error, callback_user user)
{
    test_parser_user *const actual_user = user;
    REQUIRE(actual_user->expected_count >= 1);
    REQUIRE(parse_error_equals(actual_user->expected_errors[0], error));
    --actual_user->expected_count;
    ++actual_user->expected_errors;
}
