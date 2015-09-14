#!/bin/sh

# make clean
# make moz

sh tool/gen.sh
LOOP=5
if [ $# = 1 ]; then
    LOOP=$1
fi
BENCH=test_vm/bench/input
CMD="./Release/moz -n ${LOOP} -q -s "
# CMD2="./Release/moz_profile -n 1 -q -s "
echo "citys.json"
${CMD} -p sample/old_json.ncz -i ${BENCH}/citys.json
echo "earthquake.geojson"
${CMD} -p sample/old_json.ncz -i ${BENCH}/earthquake.geojson
echo "benchmark4.json"
${CMD} -p sample/old_json.ncz -i ${BENCH}/benchmark4.json
echo "xmark5m.xml"
${CMD} -p sample/xml.ncz  -i ${BENCH}/xmark5m.xml

# ${CMD2} -p sample/old_json.ncz -i ${BENCH}/earthquake.geojson
# ${CMD2} -p sample/xml.ncz  -i ${BENCH}/xmark5m.xml
