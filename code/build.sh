#!/bin/bash

if [ $# == 0 ] ; then
    echo 'Usage: build Release| Debug| RelWithDebInfo| MinSizeRel [-g]'
    exit
fi

mkdir ../../build
mkdir ../../build/handmade
pushd ../../build/handmade

if [ "$2" == '-g' ] ; then
   cmake ../../handmade/
fi
cmake --build . --config $1
popd
