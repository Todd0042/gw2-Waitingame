#!/usr/bin/env bash
set -e
cmake -B build -DCMAKE_TOOLCHAIN_FILE=mingw-toolchain.cmake -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
echo "Done: build/waitingame.dll"
