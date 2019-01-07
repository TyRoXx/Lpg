var globalObject = new Function('return this;')();
var fail = function (condition) {
    if (globalObject.builtin_fail) {
        builtin_fail();
    } else {
        throw "fail";
    }
};
var assert = function (condition) {
    if (globalObject.builtin_assert) {
        builtin_assert(condition);
    }
    else if (!condition) {
        fail();
    }
};
var string_equals = function (left, right) {
    return (left === right);
};
var integer_equals = function (left, right) {
    return (left === right);
};
var integer_less = function (left, right) {
    return (left < right);
};
var integer_subtract = function (left, right) {
    var difference = (left - right);
    return (difference < 0) ? undefined : difference;
};
var integer_add = function (left, right) {
    return (left + right);
};
var integer_add_u32 = function (left, right) {
    var sum = (left + right);
    return (sum <= 0xffffffff) ? sum : undefined;
};
var integer_add_u64 = function (left, right) {
    var sum = (left + right);
    return sum /*TODO support more than 53 bits*/;
};
var integer_and_u64 = function (left, right) {
    var result = (left & right);
    return result /*TODO support more than 53 bits*/;
};
var concat = function (left, right) {
    return (left + right);
};
var not = function (argument) {
    return !argument;
};
var side_effect = function () {
};
var integer_to_string = function (input) {
    return "" + input;
};
