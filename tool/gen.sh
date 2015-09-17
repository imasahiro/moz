#!/bin/sh

NEZOPT=--option:+ast:+memo:+predict:+str
NEZ=../../nez.jar

cd sample
for i in *.nez; do
    echo $i
    java -jar ${NEZ} compile ${NEZOPT} -p $i
done
