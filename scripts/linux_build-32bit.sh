#!/bin/bash

# choose the same values as in CI
BUILD_TYPE=Debug      # or Release
GCC_VER=12            # match matrix.compiler

# 1) Create build directory
cmake -E make_directory ../build

# 2) Configure (32-bit) – EXACTLY like CI
cd ../build
cmake -GNinja \
  -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
  -DCMAKE_CXX_COMPILER=g++-${GCC_VER} \
  -DCMAKE_C_FLAGS="-m32" \
  -DCMAKE_CXX_FLAGS="-m32" \
  -DCMAKE_EXE_LINKER_FLAGS="-m32" \
  ..

# 3) Build
cmake --build .
