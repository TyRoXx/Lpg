#include "handle_parse_error.h"
#include "test.h"

void handle_error(parse_error const error, callback_user user)
{
    test_parser_user *const parser_user = user;
    REQUIRE(parser_user->expected_count >= 1);
    REQUIRE(parse_error_equals(parser_user->expected_errors[0], error));
    --parser_user->expected_count;
    ++parser_user->expected_errors;
}
