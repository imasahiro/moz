#!/bin/sh

# make clean
make moz

LOOP=5
if [ $# == 1 ]; then
    LOOP=$1
fi
BENCH=test_vm/bench/input
CMD="./build/moz -n ${LOOP} -q -s "
echo "citys.json"
${CMD} -p sample/json.nzc -i ${BENCH}/citys.json
echo "earthquake.geojson"
${CMD} -p sample/json.nzc -i ${BENCH}/earthquake.geojson
echo "benchmark4.json"
${CMD} -p sample/json.nzc -i ${BENCH}/benchmark4.json
echo "xmark5m.xml"
${CMD} -p sample/xml.nzc  -i ${BENCH}/xmark5m.xml
