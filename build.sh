#!/bin/sh

clang -g -fPIC -shared reader.cpp -Wno-c++11-compat-deprecated-writable-strings -lcurl -o libreader.so
clang -g main.cpp -Wno-c++11-compat-deprecated-writable-strings -o reader libreader.so -Wl,-rpath,/home/joe/work/rss_reader 

