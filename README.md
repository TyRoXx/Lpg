[![codecov](https://codecov.io/gh/mamazu/Lpg/branch/master/graph/badge.svg)](https://codecov.io/gh/mamazu/Lpg)
[![Build Status](https://travis-ci.org/mamazu/Lpg.svg?branch=master)](https://travis-ci.org/mamazu/Lpg)

# Lpg
1. [Usage](#Usage)
    1. [Syntax](#Syntax)
        1. [Types](#Types)
        1. [Variables and constants](#Variables-and-constants)
        1. [Functions](#Functions)
        1. [Match](#Match)
        1. [Loops](#Loops)
        1. [Tuple](#Tuple)
    1. [Compilation](#Compilation)
1. [Development](#Development)
    1. [Build and run on Ubuntu](#Build-and-run-on-Ubuntu)
    1. [Build and run on Windows](#Build-and-run-on-Windows)

## Usage
Just write a `.lpg` file and run it. If you want to have syntax highlighting, there is a Visual Studio Code Plugin for this (see VS Code Package Manager) and a Notepad++ syntax file in this repository.

### Syntax

#### Types

| Type           | Explanation                                      | Example values                    |
|----------------|--------------------------------------------------|-----------------------------------|
| unit           | A nothing like type "void"                       | `unit`                            |
| type           | A type                                           | `unit`, `boolean`, `int(0, 0)`    |
| boolean        | A logical value (either true or false)           | `boolean.true` or `boolean.false` |
| int(low, high) | A whole number within a certain range            | `int(0, 100)` or `int(-100, 33)`  |
| string-ref     | A reference to a string that is a string literal | `"Hello"` or `"\n"`               |

#### Variables and constants
* Variables

**Currently there all values are constants.**

* Constants

You can declare a constant with `let a = 10`. This will implicitly set the type of the constant to be an integer. If you want to have a specific type of constant use `let a : int(0, 0) = 0`. This creates a constant with a type of integer with the range between 0 and 0.

#### Functions
Functions can be implemented like this
```lpg
()
    print("Hello World")
    return 1
```
This is an **anonymous function** which prints "Hello World" to the screen and returns 1 to the caller. If you want to give the function a name, you have to save this in a constant. Like so:
```lpg
let print_int = (in : int(0, 100))
    print(in)
```

If the **functions just consists of a return statement**, you can also abbreviate it like that:
```lpg
let xor = (left : boolean, right : boolean) and(or(left, right), not(and(left, right))) 
```

There is no possibility of defining the return type of a function. This will be determined implicitly.

**Calling a function** is as easy as writing the function name and then the parameters (if there are any).
```lpg
xor(boolean.true, boolean.false)
``` 

#### Match
The match statement works similar to the switch statement in other languages.


TODO!!!!

#### Loops
You can create a loop with the simple loop key word. By default all loops will run infinitely long if there is no break. So this will print "Hello World" for ever:
```lpg
loop
    print("Hello World")
```

You can combine this with the `match`-statement in order to break out of a loop in a controlled manner.

#### Tuple
Tuples are a collection of values. They do not have to have the same type to be stored together. An example of a tuple would be:
```lpg
let tuple = {10, boolean.true, "Hello", () boolean}
```
The type of the variable is then automatically derived. In order to access an element of a tuple you write the tuple's name and the index you want to access. Like for example `tuple.2` would give you the `string-ref "Hello"`.

### Compilation
In order to compile and run an `.lpg` file you need to build the whole project and run the following command in the terminal:

* On Windows:
> `{build-directory}/cli/lpg.exe` and give it the `.lpg` file as argument.

* On Linux
> `{build-directory}/cli/lpg` and give it the `.lpg` file as argument.

## Development
Currently supported operating systems are:
* Windows
* Linux

### Build and run on Ubuntu
Install at least the following tools:
```bash
sudo apt-get install git cmake gcc
```

How to build and run the tests:
```bash
git clone https://github.com/TyRoXx/Lpg.git
mkdir build
cd build
cmake ../Lpg -G "CodeBlocks - Unix Makefiles"
make
./tests/tests
```

You can also open the project using an IDE. QtCreator is recommended:
```bash
sudo apt-get install qtcreator
```

### Code formatting
Install `clang-format` like this on Ubuntu:
```bash
sudo apt-get install clang-format-3.7
```

On Windows, you can get clang-format by installing Clang for Windows:

* http://releases.llvm.org/3.7.1/LLVM-3.7.1-win64.exe
* http://releases.llvm.org/download.html#3.7.1

You have to use clang-format 3.7 or otherwise the formatting will be inconsistent.

Re-run `cmake` in the build directory to make it will look for `clang-format`:
```bash
cmake .
```

To format, run this in the build directory:
```bash
make clang-format
```

### Build and run on Windows
TODO!