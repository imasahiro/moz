#!/bin/sh

for dir in Debug Release; do
    rm -rf $dir
    mkdir -p $dir
    (cd $dir && \
        cmake .. -DCMAKE_BUILD_TYPE=$dir && \
        make)
done
