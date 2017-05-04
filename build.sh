#!/bin/sh

clang -g main.cpp -Wno-c++11-compat-deprecated-writable-strings -lcurl -o reader
