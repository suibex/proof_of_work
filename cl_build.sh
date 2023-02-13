#!/bin/bash
clang++ hashing_cl.cc -o blah -std=c++11 -lssl -lcrypto -lpthread -lOpenCL -L/usr/lib/x86_64-linux-gnu/libOpenCL.so;
./blah;
