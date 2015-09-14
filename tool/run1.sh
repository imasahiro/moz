#!/bin/sh

make -C Debug clean
make -C Debug
FILE=$1
TXT=$2
# M=valgrind --leak-check=full
# java -jar ../nez.jar compile -p $FILE

# lldb -- ./Debug/moz -p ${FILE%.*}.ncz -i $2
# ./Debug/moz -p ${FILE%.*}.ncz -i $2 >& out
valgrind --leak-check=full ./Debug/moz -p ${FILE%.*}.ncz -i $2
