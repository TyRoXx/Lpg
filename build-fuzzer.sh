#!/usr/bin/env sh
# run this from the root of the repository and pass the CMake build directory for the fuzzer as the first argument
BUILD_DIR=$1
SOURCE_DIR=`pwd`
mkdir -p $BUILD_DIR || exit 1
cd $BUILD_DIR || exit 1
CC=clang CXX=clang++ cmake -DCMAKE_BUILD_TYPE=Debug -DLPG_WITH_CLANG_FUZZER=ON -DLPG_WITH_DUKTAPE=ON -G "Ninja" $SOURCE_DIR || exit 1
cmake --build . || exit 1
