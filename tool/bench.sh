#!/bin/sh

make clean
make

echo "benchmark4.json"
time -p ./build/moz -p sample/json.nzc -i ~/src/peg/benchNezVM/input/benchmark4.json
time -p ./build/moz -p sample/json.nzc -i ~/src/peg/benchNezVM/input/benchmark4.json
time -p ./build/moz -p sample/json.nzc -i ~/src/peg/benchNezVM/input/benchmark4.json
time -p ./build/moz -p sample/json.nzc -i ~/src/peg/benchNezVM/input/benchmark4.json
