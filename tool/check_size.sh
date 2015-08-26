#!/bin/sh

make -C Release clean
make -C Release >/dev/null 2>&1
time -p ./Release/moz_stat -s -q -n 1 -p sample/json.nzc -i test_vm/bench/input/benchmark4.json
echo
time -p ./Release/moz_stat -s -q -n 1 -p sample/js.nzc -i nez-sample/_/js/jquery-2.1.1.js
echo -n "linux gcc4.7.2 x86 -Os      :"
du -h ./Release/moz_all
strip ./Release/moz_all
echo -n "linux gcc4.7.2 x86 -Os+strip:"
du -h ./Release/moz_all
