#!/bin/sh

# TODO(joe): Make this build in a build directory.

clang -g -fPIC -shared reader.cpp -Wno-c++11-compat-deprecated-writable-strings -o libreader.so
clang -g main.cpp -Wno-c++11-compat-deprecated-writable-strings -lcurl -o reader libreader.so -Wl,-rpath,/home/joe/work/rss_reader/ 
