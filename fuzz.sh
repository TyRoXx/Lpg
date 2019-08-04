#!/usr/bin/env sh
# run this from the root of the repository and pass the CMake build directory as the first argument
BUILD_DIR=$1
./build-fuzzer.sh $BUILD_DIR || exit 1
SOURCE_DIR=`pwd`
cd "$BUILD_DIR/fuzz" || exit 1
./fuzz -timeout=20 -only_ascii=1 $SOURCE_DIR/fuzz/corpus
