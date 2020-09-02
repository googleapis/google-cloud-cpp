#!/bin/bash
export LIBRARY_PATH=$LIBRARY_PATH:/usr/lib 
clang -L/usr/local/lib -std=c++11 -isystem /Users/tina/dev/google-cloud-cpp -isystem /Users/tina/dev/install/include google/cloud/storage/client_example.cc
