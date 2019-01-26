var globalObject = new Function('return this;')();
var fail = function () {
    if (globalObject.builtin_fail) {
        builtin_fail();
    } else {
        throw "fail";
    }
};
var assert = function (condition) {
    if (typeof(condition) !== "boolean") {
        fail();
    }
    if (globalObject.builtin_assert) {
        builtin_assert(condition);
    }
    else if (!condition) {
        fail();
    }
};
var string_equals = function (left, right) {
    assert(typeof(left) === "string");
    assert(typeof(right) === "string");
    return (left === right);
};
var integer_equals = function (left, right) {
    if (typeof(left) === "number") {
        if (typeof(right) === "number") {
            return (left === right);
        } else if (typeof(right) === "object") {
            return ((left >> 32) === right[0]) && ((left & 0xffffffff) === right[1]);
        } else {
            fail();
        }
    } else if (typeof(left) === "object") {
        if (typeof(right) === "number") {
            return (left[0] === (right >> 32)) && (left[1] === (right & 0xffffffff));
        } else if (typeof(right) === "object") {
            return (left[0] === right[0]) && (left[1] === right[1]);
        } else {
            fail();
        }
    } else {
        fail();
    }
};
var integer_less = function (left, right) {
    assert(typeof(left) === "number");
    assert(typeof(right) === "number");
    return (left < right);
};
var integer_subtract = function (left, right) {
    assert(typeof(left) === "number");
    assert(typeof(right) === "number");
    var difference = (left - right);
    return (difference < 0) ? undefined : difference;
};
var integer_add = function (left, right) {
    assert(typeof(left) === "number");
    assert(typeof(right) === "number");
    return (left + right);
};
var integer_add_u32 = function (left, right) {
    assert(typeof(left) === "number");
    assert(typeof(right) === "number");
    var sum = (left + right);
    return (sum <= 0xffffffff) ? sum : undefined;
};
var integer_add_u64 = function (left, right) {
    assert(typeof(left) === "number");
    assert(typeof(right) === "number");
    var sum = (left + right);
    return sum /*TODO support more than 53 bits*/;
};
var normalize_u32 = function (denormalized) {
    assert(typeof(denormalized) === "number");
    if(denormalized < 0) {
        return 0x100000000 + denormalized;
    }
    return denormalized;
}
var normalize_u64 = function (denormalized) {
    assert(typeof(denormalized) === "number");
    if(denormalized < 0) {
        return [0xffffffff, (denormalized >>> 0)];
    }
    return denormalized;
}
var make_u64 = function (high, low) {
    if (high === 0) {
        return normalize_u32(low);
    }
    return [normalize_u32(high), normalize_u32(low)];
};
var integer_and_u64 = function (left, right) {
    if (typeof(left) === "number") {
        if (typeof(right) === "number") {
            var result = (left & right);
            return result;
        } else if (typeof(right) === "object") {
            return make_u64(0, left & right[1]);
        } else {
            fail();
        }
    } else if (typeof(left) === "object") {
        if (typeof(right) === "number") {
            return make_u64(0, left[1] & right);
        } else if (typeof(right) === "object") {
            return make_u64(left[0] & right[0], left[1] & right[1]);
        } else {
            fail();
        }
    } else {
        fail();
    }
};
var integer_or_u64 = function (left, right) {
    if (typeof(left) === "number") {
        if (typeof(right) === "number") {
            var result = (left | right);
            return result;
        } else if (typeof(right) === "object") {
            return make_u64(right[0], left | right[1]);
        } else {
            fail();
        }
    } else if (typeof(left) === "object") {
        if (typeof(right) === "number") {
            return make_u64(left[0], left[1] | right);
        } else if (typeof(right) === "object") {
            return make_u64(left[0] | right[0], left[1] | right[1]);
        } else {
            fail();
        }
    } else {
        fail();
    }
};
var integer_xor_u64 = function (left, right) {
    if (typeof(left) === "number") {
        if (typeof(right) === "number") {
            var result = (left ^ right);
            return result;
        } else if (typeof(right) === "object") {
            return make_u64(right[0], left ^ right[1]);
        } else {
            fail();
        }
    } else if (typeof(left) === "object") {
        if (typeof(right) === "number") {
            return make_u64(left[0], left[1] ^ right);
        } else if (typeof(right) === "object") {
            return make_u64(left[0] ^ right[0], left[1] ^ right[1]);
        } else {
            fail();
        }
    } else {
        fail();
    }
};
var integer_not_u64 = function (input) {
    if (typeof(input) === "number") {
        return normalize_u64(~input);
    } else if (typeof(input) === "object") {
        return make_u64(~input[0], ~input[1]);
    } else {
        fail();
    }
};
var todo = fail;
var integer_shift_left_u64 = function (left, right) {
    if (typeof(left) === "number") {
        if (typeof(right) === "number") {
            return (left << right);
        } else if (typeof(right) === "object") {
            todo();
        } else {
            fail();
        }
    } else if (typeof(left) === "object") {
        if (typeof(right) === "number") {
            return make_u64((left[0] << right) | (left[1] >>> (32 - right)), left[1] << right);
        } else if (typeof(right) === "object") {
            todo();
        } else {
            fail();
        }
    } else {
        fail();
    }
};
var concat = function (left, right) {
    assert(typeof(left) === "string");
    assert(typeof(right) === "string");
    return (left + right);
};
var not = function (argument) {
    assert(typeof(argument) === "boolean");
    return !argument;
};
var side_effect = function () {
};
var integer_to_string = function (input) {
    assert(typeof(input) === "number");
    return "" + input;
};
