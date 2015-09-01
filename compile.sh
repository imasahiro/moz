#!/bin/bash

if [ -n $1 -a -e $1 ]; then
	gcc `llvm-config --cflags` -c $1 -o tmp.o
	g++ tmp.o `llvm-config --cxxflags --ldflags --libs core executionengine jit interpreter analysis native bitwriter --system-libs`
	rm tmp.o
fi
