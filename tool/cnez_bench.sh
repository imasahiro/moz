#!/bin/sh

# make clean
# make moz

LOOP=5
if [ $# = 1 ]; then
    LOOP=$1
fi
# OPT=--option:+packrat:-predict
BENCH=test_vm/bench/input
JSON=json.c
XML=xml.c
JSON_BIN=${JSON%.*}.exe
XML_BIN=${XML%.*}.exe

java -jar ../nez.jar cnez ${OPT} -p sample/json.nez
java -jar ../nez.jar cnez ${OPT} -p sample/xml.nez
clang -O3 -Isrc -I. -LRelease -lnez ${JSON} -o ${JSON_BIN}
clang -O3 -Isrc -I. -LRelease -lnez ${XML}  -o ${XML_BIN}
JSON_CMD="./${JSON_BIN} -d -n ${LOOP} -q -s "
XML_CMD="./${XML_BIN}   -d -n ${LOOP} -q -s "

echo "citys.json"
${JSON_CMD} -i ${BENCH}/citys.json
echo "earthquake.geojson"
${JSON_CMD} -i ${BENCH}/earthquake.geojson
echo "benchmark4.json"
${JSON_CMD} -i ${BENCH}/benchmark4.json
# echo "xmark5m.xml"
# ${XML_CMD} -i ${BENCH}/xmark5m.xml
