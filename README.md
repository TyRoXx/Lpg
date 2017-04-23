[![codecov](https://codecov.io/gh/TyRoXx/Lpg/branch/master/graph/badge.svg)](https://codecov.io/gh/TyRoXx/Lpg)
[![Build Status](https://travis-ci.org/TyRoXx/Lpg.svg?branch=master)](https://travis-ci.org/TyRoXx/Lpg)
[![Build status](https://ci.appveyor.com/api/projects/status/lq9sc1am1xn5fvgg/branch/master?svg=true)](https://ci.appveyor.com/project/TyRoXx/lpg/branch/master)

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

Re-run `cmake` in the build directory to make it will look for `clang-format`:

    cmake .

To format, run this in the build directory:

    make clang-format
