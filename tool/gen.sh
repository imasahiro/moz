#!/bin/sh

NEZOPT=--option:+ast:-memo:-predict
NEZ=../nez.jar

for i in sample/*.nez; do
    java -jar ${NEZ} compile ${NEZOPT} -p $i
done
