#!/bin/sh

make moz
FILE=$1
# M=valgrind --leak-check=full
java -jar ../nez.jar compile -p $FILE

lldb -- ./build/moz -p ${FILE%.*}.nzc -i ${FILE%.*}.txt
# valgrind --leak-check=full ./build/moz -p ${FILE%.*}.nzc -i ${FILE%.*}.txt
