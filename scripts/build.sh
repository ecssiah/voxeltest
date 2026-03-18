#!/usr/bin/env sh

set -e

BUILD_TYPE=${1:-Debug}

echo "Configuring ($BUILD_TYPE)..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=$BUILD_TYPE

echo "Building..."
cmake --build build -j

echo "Generating tags..."
ctags -e -R .

echo "Running..."
./build/voxeltest
