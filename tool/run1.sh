#!/bin/sh

make -C Debug clean
make -C Debug
FILE=$1
TXT=$2
# M=valgrind --leak-check=full
# java -jar ../nez.jar compile -p $FILE

lldb -- ./Debug/moz -p ${FILE%.*}.nzc -i $2
# ./Debug/moz -p ${FILE%.*}.nzc -i $2 >& out
# valgrind --leak-check=full ./build/moz -p ${FILE%.*}.nzc -i ${FILE%.*}.txt
