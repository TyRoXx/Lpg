# Language overview

1. [Hello World](#Hello-World)
1. Types and syntax
    1. [Variables and constants](#Variables-and-constants)
    1. [Functions](#Function-Syntax)
    1. [Interfaces](#Interfaces)
1. Concepts
    1. [Match](#Match)
    1. [Loops](#Loops)
    1. [Comments](#Comments)
    1. [Modules](#Modules)
1. [Standard Library Functions](#Standard-library-functions)
1. [Standard Library Constants](#Standard-library-constants)
1. [Built-in global functions](#Built-in-global-functions)
1. [Optimizations](#Optimizations)

## Hello World
The first thing in every programming language is the "Hello World"-program. This is the code you need to write in lpg: **It does not exist yet.**

## Built-in types

| Type            | Explanation                                      | Example values                                   |
|-----------------|--------------------------------------------------|--------------------------------------------------|
| unit            | A nothing like type "void"                       | `unit_value`                                     |
| int(low, high)  | A whole number within a certain range            | `int(0, 100)` or `int(33, 100)`                  |
| string          | A reference to a string that is a string literal | `"Hello"` or raw string: `'\n'`                  |
| *no syntax yet* | Contains a fixed number of elements              | `{"Hello"}` or `{}` or `{boolean.true, 123, {}}` |


### Variables and constants
You can declare a constant with `let a = 10`. This will implicitly set the type of the constant to be an integer. If you want to have a specific type of constant use `let a: int(0, 0) = 0`. This creates a constant with a type of integer with the range between 0 and 0.

In order to make a constant mutable you need to define it as mutable:
```lpg
let integer = import integer.integer
let std = import std

let i: int(5, 5) = 5
let some_int: std.mutable[integer] = std.make_mutable[integer](i)

// Set variable content
some_int.store(10)

// Load content
some_int.load()
```

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
let return_argument = (message: std.string)
    message
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

### Arrays
Opposite to tuples arrays are a list of elements with the same type. Arrays have a template type `array[T]` where T is the type of the elements in the array. So a list of integers would have type `array[integer]`. A simple example how to use arrays is here:
```lpg
let std = import std 
let string_list : std.array[std.string] = new_array(std.string)

// Adding elements
string_list.append("something")

// Getting the size of the array
string_list.size()

// Setting element at index 0, it returns true on sucess
string_list.store(0, "hello")
```
Those are the basic functions of an array. Loading from the array is a little more complicated because it returns a `std.option` which has either a value if the index exists or none if the index does not exist.
```lpg
let std = import std
let string = std.string

let string_list : std.array[string] = new_array(string)
string_list.append("something")

match string_list.load(0)
    case std.option[string].some(let element):
        string_equals(element, "something")
    case std.option[string].none:
        boolean.false
```

### Interfaces
Like in other programming languages LPG also offers the user to define interfaces and implement them later on. Here is an example how to define an interface. However there is no type for this variable.
```lpg
let make_percent = interface
    percent(): int(0, 100)
```

And this is how you implement this:
```lpg
let string = import std.string

let printable = interface
    print(): string

impl printable for string
    print(): string
        self
```

And this is how you use it:
```lpg
let make_percent = interface
    percent(): int(0, 100)

let print_percent = (something: make_percent)
    integer_to_string(something.percent())
```

### Structures
Structures provide the tool to define your own data structures. Like so:
```lpg
let std = import std

let t : std.type = struct
    a: std.boolean

let t_instance : t = t{std.boolean.true}
```

When you want to nest a struct inside a struct, you can do this as well.
```lpg
let std = import std

let t : std.type = struct
    a: std.boolean

let u = struct
    a: t
    b: std.string

let u_instance = u{t{std.boolean.true}, "abc"}
```

## Concepts

### Match
The match expression works similar to the switch statement in other languages.
Match supports enumerations, integers and strings.

```
let bool = import std.boolean

let a = bool.true
let result : int(1, 2) = match a
    case bool.false:
        1
    case bool.true:
        2

assert(integer_equals(2, result))
```

(Note that LPG does not currently support default cases for enums.)

Matching stateful enums works like this:
```
let option = import std.option
let result : int(1, 2) = match option[boolean].some(boolean.true)
    case option[boolean].some(let e):
        assert(e)
        1
    case option[boolean].none:
        2
assert(integer_equals(1, result))
```

Example for match with a string:

```
let result : int(1, 3) = match "a"
    case "a":
        1
    case "b":
        2
    default: // a match has to be exhaustive therefore strings always need a default case
        3
assert(integer_equals(1, result))
```

`match` does not support `break` or fall-through.
The last expression in a `case` is returned as the result of the `match` expression.
The result can be `unit` if you don't want to return anything.

### Loops
You can create a loop with the simple loop key word. By default all loops will run infinitely long if there is no break. So this will print "Hello World" for ever:
```lpg
let bool = import std.boolean

loop
    assert(bool.true)
```
If you don't want to have an infinite loop, you can exit the loop at any time with the `break` keyword. Like for example this loop runs as long as the function returns true:
```lpg
let std = import std
let bool = std.boolean

let a = bool.true
loop
    let still_running = match a
        case bool.false:
            break
            std.unit
        case bool.true:
            std.unit
assert(bool.true)
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

### Modules

#### Importing modules
Most of the functions can be found in the standard libary and have to be imported when used in the current LPG file. To import a module you need to specify the module name and the handle.
```lpg
let handle = import std
```
For importing only parts of the modules you can use the dot syntax
```lpg
let string = import std.string
```

#### Writing modules

When writing a file that should expose functions or types to other files the syntax is:
```lpg
let fav_num = 7

// Define structure of exported value
let export_struct = struct
    fav_num: type_of(fav_num)

// Instanciate exported value
export_struct{fav_num}
```
which basically is the return value of the whole file.

## Standard library functions
The standard library also includes some functions to work with the types.
```lpg
let std = import std
```

| Name | Inputs           | Explanation                    | Output  |
|------|------------------|--------------------------------|---------|
| and  | boolean, boolean | Implementation of logical and  | boolean |
| or   | boolean, boolean | Implementation of logical or   | boolean |
| not  | boolean          | Flips the value of the boolean | boolean |

## Standard library constants
```lpg
let std = import std
```

| Name       | Explanation                                                                                  |
|------------|----------------------------------------------------------------------------------------------|
| type       | type of all types                                                                            |
| string     | name of the string type                                                                      |
| boolean    | an enum with two states: false, true                                                         |
| unit       | type with a single value that can be used to model the absence of a return value for example |
| unit_value | the single possible value of type unit                                                       |
| option[T]  | enum with two states: `some(T)` and `none`                                                   |
| option-int | deprecated                                                                                   |
| array[T]   | generic interface for arrays of type T                                                       |

## Built-in global functions
```lpg
assert(boolean.true)
```

| Name              | Inputs           | Explanation                                          | Output  |
|-------------------|------------------|------------------------------------------------------|---------|
| assert            | boolean          | Ends the program if the input is `boolean.false`     | unit    |
| concat            | string, string   | Returns the two strings together                     | string  |
| string_equals     | string, string   | Returns if two strings are equal                     | boolean |
| integer_equals    | integer, integer | Returns if two integers are equals                   | boolean |
| integer_less      | integer, integer | Returns if the first integer is less than the second | boolean |
| integer_to_string | integer          | Turns an integer to a string                         | string  |

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
