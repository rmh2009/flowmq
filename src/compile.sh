#!/bin/bash
MAIN=$1
echo "compiling $1.cpp"
g++ -o $MAIN.tsk $MAIN.cpp  \
    /Users/hong/usr/boost-1.68.0/lib/libboost_system.a \
    -I/Users/hong/usr/boost-1.68.0/include \
    -I/Users/hong/github_projects/flowmq/src \
    -std=c++11 -I./

