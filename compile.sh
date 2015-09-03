#!/bin/bash

#if [ -e $1 ]; then
#	g++ -c $1 -o 2.out `llvm-config --cxxflags --ldflags --libs` -ldl -lpthread
#fi
if [ -n $1 -a -e $1 ]; then
	g++ $1 `llvm-config --cxxflags --ldflags --libs` -ldl -lpthread
fi
