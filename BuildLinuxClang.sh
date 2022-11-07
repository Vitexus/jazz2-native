#!/usr/bin/env bash

BuildType=Release
#BuildType=Debug

export OS=linux
export CC=clang
export CXX=clang++

cmake -B ../Jazz2-LinuxClang-${BuildType} -D CMAKE_BUILD_TYPE=${BuildType} -D NCINE_LINKTIME_OPTIMIZATION=ON
make -j8 -C ../Jazz2-LinuxClang-${BuildType}