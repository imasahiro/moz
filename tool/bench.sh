#!/bin/sh

# make clean
# make moz

sh tool/gen.sh
LOOP=5
if [ $# = 1 ]; then
    LOOP=$1
fi
EXT=moz
BENCH=test_vm/bench/input
CMD="./Release/moz -n ${LOOP} -q -s "
# CMD2="./Release/moz_profile -n 1 -q -s "
echo "citys.json"
${CMD} -p sample/old_json.${EXT} -i ${BENCH}/citys.json
echo "earthquake.geojson"
${CMD} -p sample/old_json.${EXT} -i ${BENCH}/earthquake.geojson
echo "benchmark4.json"
${CMD} -p sample/old_json.${EXT} -i ${BENCH}/benchmark4.json
echo "xmark5m.xml"
${CMD} -p sample/xml.${EXT}  -i ${BENCH}/xmark5m.xml

# ${CMD2} -p sample/old_json.${EXT} -i ${BENCH}/earthquake.geojson
# ${CMD2} -p sample/xml.${EXT}  -i ${BENCH}/xmark5m.xml
