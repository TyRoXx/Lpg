[![codecov](https://codecov.io/gh/mamazu/Lpg/branch/master/graph/badge.svg)](https://codecov.io/gh/mamazu/Lpg)
[![Build Status](https://travis-ci.org/mamazu/Lpg.svg?branch=master)](https://travis-ci.org/mamazu/Lpg)

# Lpg

## Build and run on Ubuntu
Install at least the following tools:

    sudo apt-get install git cmake gcc

How to build and run the tests:

    git clone https://github.com/TyRoXx/Lpg.git
    mkdir build
    cd build
    cmake ../Lpg -G "CodeBlocks - Unix Makefiles"
    make
    ./tests/tests

You can also open the project using an IDE. QtCreator is recommended:

    sudo apt-get install qtcreator

## Code formatting
Install `clang-format` like this on Ubuntu:

    sudo apt-get install clang-format-3.7

On Windows, you can get clang-format by installing Clang for Windows:

* http://releases.llvm.org/3.7.1/LLVM-3.7.1-win64.exe
* http://releases.llvm.org/download.html#3.7.1

You have to use clang-format 3.7 or otherwise the formatting will be inconsistent.

Re-run `cmake` in the build directory to make it will look for `clang-format`:

    cmake .

To format, run this in the build directory:

    make clang-format
