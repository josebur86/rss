#!/bin/sh

if [ ! -d "build" ]; then
    mkdir build
fi

clang -g -fPIC -shared reader.cpp -Wno-c++11-compat-deprecated-writable-strings -lcurl -o ./build/libreader.so
clang -g main.cpp -Wno-c++11-compat-deprecated-writable-strings -o ./build/reader ./build/libreader.so -Wl,-rpath,/home/joe/work/rss_reader/build 

