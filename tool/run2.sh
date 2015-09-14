#!/bin/sh

# make -C Debug clean
make -C Debug
FILE=$1
TXT=$2
# M=valgrind --leak-check=full
java -jar ../nez.jar compile --option:-memo -p $FILE

# lldb -- ./Debug/moz -p ${FILE%.*}.ncz -i $2
# ./Debug/moz -p ${FILE%.*}.ncz -i $2 >& out
# valgrind --leak-check=full ./Debug/moz -p ${FILE%.*}.ncz -i $2
echo ${FILE%.*}.ncz
echo ${FILE%.*}.txt
lldb -- ./Debug/moz -p ${FILE%.*}.ncz -i ${FILE%.*}.txt
# ./Debug/moz -p ${FILE%.*}.ncz -i ${FILE%.*}.txt
