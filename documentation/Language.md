# Language overview

1. [Hello World](#Hello-World)
1. Types and syntax
    1. [Variables and constants](#Variables-and-constants)
    1. [Functions](#Function-Syntax)
    1. [Tuple](#Tuples)
    1. [Interfaces](#Interfaces)
1. Concepts
    1. [Match](#Match)
    1. [Loops](#Loops)
    1. [Comments](#Comments)
1. [Standard Library Functions](#Standard-library-functions)
1. [Standard Library Constants](#Standard-library-constants)
1. [Built-in global functions](#Built-in-global-functions)
1. [Optimizations](#Optimizations)

## Hello World
The first thing in every programming language is the "Hello World"-program. This is the code you need to write in lpg: **It does not exist yet.**

## Types

| Type            | Explanation                                      | Example values                                   |
|-----------------|--------------------------------------------------|--------------------------------------------------|
| unit            | A nothing like type "void"                       | `unit_value`                                     |
| type            | A type of a variable / constant                  | `unit`, `boolean`, `int(0, 0)`                   |
| boolean         | A logical value (either true or false)           | `boolean.true` or `boolean.false`                |
| int(low, high)  | A whole number within a certain range            | `int(0, 100)` or `int(33, 100)`                  |
| string-ref      | A reference to a string that is a string literal | `"Hello"` or `"\n"`                              |
| *no syntax yet* | Contains a fixed number of elements              | `{"Hello"}` or `{}` or `{boolean.true, 123, {}}` |


### Variables and constants
* Variables

**Currently there all variables are constants.**

* Constants

You can declare a constant with `let a = 10`. This will implicitly set the type of the constant to be an integer. If you want to have a specific type of constant use `let a: int(0, 0) = 0`. This creates a constant with a type of integer with the range between 0 and 0.

### Function Syntax
Functions can be implemented like this. Currently all functions are lambda (nameless) functions:
```lpg
()
    assert(boolean.true)
    1
```
If a line just consists of one value that is not assigned to anything and it is the last line of the function, then it is **the return value** of the function. On the other hand you can also be more specific and use the return keyword. Which ensures no matter where that this is the return value of the function and execution of everything after that is stopped.

If you want to give the function a name, you have to save this in a constant. Like so:
```lpg
let std = import std
let duplicate = (message: std.string-ref)
    {message, message}
```

If the **functions just consists of a return statement**, you can also abbreviate it like that:
```lpg
let std = import std
let xor = (left: std.boolean, right: std.boolean) std.and(std.or(left, right), std.not(std.and(left, right)))
```

There is no possibility of defining the return type of a function. This will be determined implicitly.

**Calling a function** is as easy as writing the function name and then the parameters (if there are any).
```lpg
let std = import std
let xor = (left: std.boolean, right: std.boolean) std.and(std.or(left, right), std.not(std.and(left, right)))

xor(std.boolean.true, std.boolean.false)
```

### Tuples
Tuples are a collection of values. They do not have to have the same type to be stored together. An example of a tuple would be:
```lpg
let std = import std
let tuple = {10, std.boolean.true, "Hello", () std.boolean}
```
The type of the variable is then automatically derived. In order to access an element of a tuple you write the tuple's name and the index you want to access. Like for example `tuple.2` would give you the `string-ref "Hello"`.

### Interfaces
Like in other programming languages LPG also offers the user to define interfaces and implement them later on. Here is an example how to define an interface. However there is no type for this variable.
```lpg
let make-percent = interface
    percent(): int(0, 100)
```

And this is how you implement this:
```lpg
let std = import std

let printable = interface
    print(): std.string-ref

impl printable for std.string-ref
    print(): std.string-ref
        self
```

And this is how you use it:
```lpg
let make-percent = interface
    percent(): int(0, 100)

let print-percent = (something: make-percent)
    integer-to-string(something.percent())
```

### Structures
Structures provide the tool to define your own data structures. Like so:
```lpg
let std = import std

let t : std.type = struct
    a: std.boolean

let t-instance : t = t{std.boolean.true}
```

When you want to nest a struct inside a struct, you can do this as well.
```lpg
let std = import std

let t : std.type = struct
    a: std.boolean

let u = struct
    a: t
    b: std.string-ref

let u-instance = u{t{std.boolean.true}, "abc"}
```

## Concepts

### Match
The match expression works similar to the switch statement in other languages.
Currently it only supports stateless enumerations, for example `boolean`.

```
let std = import std

let a = std.boolean.true
let result : int(1, 2) = match a
    case std.boolean.false:
        1
    case std.boolean.true:
        2

assert(integer-equals(2, result))
```

`match` does not support `break` or fall-through yet.
The last expression in a `case` is returned as the result of the `match` expression.
The result can be `unit` if you don't want to return anything.

### Loops
You can create a loop with the simple loop key word. By default all loops will run infinitely long if there is no break. So this will print "Hello World" for ever:
```lpg
let std = import std

loop
    assert(std.boolean.true)
```
If you don't want to have an infinite loop, you can exit the loop at any time with the `break` keyword. Like for example this loop runs as long as the function returns true:
```lpg
let std = import std

let a = std.boolean.true
loop
    let still-running = match a
        case std.boolean.false:
            break
            std.unit
        case std.boolean.true:
            std.unit
assert(std.boolean.true)
```

### Comments
There are two types of comments:
* mutli-line comment:
```lpg
/* Starting something 
End something */
```
* single line comment:
```lpg
// Some comment
```

## Standard library functions
The standard library also includes some functions to work with the types.
```lpg
let std = import std
```

| Name              | Inputs                 | Explanation                                          | Output     |
|-------------------|------------------------|------------------------------------------------------|------------|
| and               | boolean, boolean       | Implementation of logical and                        | boolean    |
| or                | boolean, boolean       | Implementation of logical or                         | boolean    |
| not               | boolean                | Flips the value of the boolean                       | boolean    |

## Standard library constants
```lpg
let std = import std
```

| Name              | Explanation                                                                                |
|-------------------|--------------------------------------------------------------------------------------------|
| type              | type of all types                                                                          |
| string-ref        | name of the string type                                                                    |
| boolean           | an enum with two states: false, true                                                       |
| unit              | type with a single value that can be used to model the absence of a return value for example |
| unit_value        | the single possible value of type unit                                                     |
| option[T]         | enum with two states: `some(T)` and `none` |
| option-int        | deprecated |
| array[T]          | generic interface for arrays of type T. Create array instances with the `new-array` keyword, e.g. `new-array(std.string-ref)` |

## Built-in global functions
```lpg
assert(boolean.true)
```

| Name              | Inputs                 | Explanation                                          | Output     |
|-------------------|------------------------|------------------------------------------------------|------------|
| assert            | boolean                | Ends the program if the input is `boolean.false`     | unit       |
| concat            | string-ref, string-ref | Returns the two strings together                     | string-ref |
| string-equals     | string-ref, string-ref | Returns if two strings are equal                     | boolean    |
| integer-equals    | integer, integer       | Returns if two integers are equals                   | boolean    |
| integer-less      | integer, integer       | Returns if the first integer is less than the second | boolean    |
| integer-to-string | integer                | Turns an integer to a string                         | string-ref |

## Optimizations
There are a few optimizations that LPG does when compiling an LPG files. Here is a list of some of them:

### Removing unused concepts
If you declare a constant, a variable or a that is never used and just assigned, LPG will notice this and remove the variable completely from the syntax tree. This also holds true for arguments passed to a function.
```lpg
let f = ()
    let a : int(1, 2) = 2
    "Hello"
```

This will be compiled to nothing as the function `f` itself was never used.
