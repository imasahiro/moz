#!/bin/sh

for dir in Debug Release Small; do
    rm -rf $dir
    mkdir -p $dir
    (cd $dir && \
        cmake .. -DCMAKE_BUILD_TYPE=$dir && \
        make)
done
