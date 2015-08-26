#!/bin/sh

make -C Release
./Release/moz_stat -s -q -n 1 -p sample/js.nzc -i nez-sample/_/js/jquery-2.1.1.js
strip ./Release/moz
du -h ./Release/moz
