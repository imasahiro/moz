#!/bin/sh

# make -C Debug clean
make -C Debug
FILE=$1
M="valgrind --leak-check=full"
M="lldb --"
# java -jar ../nez.jar compile -p $FILE

# lldb -- ./Debug/moz -p ${FILE%.*}.ncz -i $2
# ./Debug/moz -p ${FILE%.*}.ncz -i $2 >& out

$M ./Debug/mozvm -p $FILE -i ${FILE%.*}.txt
