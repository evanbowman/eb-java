#!/bin/bash

rm CMakeCache.txt
rm -r CMakeFiles/

cmake -DCMAKE_TOOLCHAIN_FILE=$(pwd)/devkitarm.cmake .
