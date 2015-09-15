#!/bin/sh

NEZOPT=--option:+ast:+memo:+predict:+str
NEZ=../nez.jar

for i in sample/*.nez; do
    echo $i
    java -jar ${NEZ} compile ${NEZOPT} -p $i
done
