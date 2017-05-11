#!/bin/sh

if [ ! -d "build" ]; then
    mkdir build
fi

cd build

clang -g -fPIC -shared ../reader.cpp -Wno-c++11-compat-deprecated-writable-strings -o libreader.so
clang -g ../main.cpp -Wno-c++11-compat-deprecated-writable-strings -lcurl -o reader libreader.so -Wl,-rpath,/home/joe/work/rss_reader/build 

cd ..
