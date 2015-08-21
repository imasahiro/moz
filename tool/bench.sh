#!/bin/sh

# make clean
make moz

BENCH=test_vm/bench/input
CMD="./build/moz -n 3 -s "
echo "citys.json"
${CMD} -p sample/json.nzc -i ${BENCH}/citys.json
echo "earthquake.geojson"
${CMD} -p sample/json.nzc -i ${BENCH}/earthquake.geojson
echo "benchmark4.json"
${CMD} -p sample/json.nzc -i ${BENCH}/benchmark4.json
# lldb -- ${CMD} -p sample/xml.nzc  -i ${BENCH}/xmark5m.xml
