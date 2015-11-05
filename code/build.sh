#!/bin/bash

if [ $# == 0 ] ; then
    echo 'Usage: build Release|Debug|RelWithDebInfo|MinSizeRel [-use_sdl]'
    exit
fi

mkdir ../../build
mkdir ../../build/handmade
pushd ../../build/handmade

if [ "$2" == '-use_sdl' ]; then
    cmake ../../handmade/ -DCMAKE_BUILD_TYPE=$1 -DCMAKE_CXX_COMPILER_ID=Clang -Duse_sdl=on
else
    cmake ../../handmade/ -DCMAKE_BUILD_TYPE=$1 -DCMAKE_CXX_COMPILER_ID=Clang -Duse_sdl=off
fi
cmake --build .
popd
