#!/bin/bash

if [ $# == 0 ] ; then
    echo 'Usage: build Release| Debug| RelWithDebInfo| MinSizeRel'
    exit
fi

mkdir ../../build
mkdir ../../build/handmade
pushd ../../build/handmade

cmake ../../handmade/ -DCMAKE_BUILD_TYPE=$1 -DCMAKE_CXX_COMPILER_ID=Clang
cmake --build .
popd
