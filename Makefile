
NEZ=../nez.jar
OPT=0
DEBUG=3
NEZ_LIB=src/bitset.c src/instruction.h src/pstring.h src/mozvm.h
RUBY=ruby

all: loader gen

loader: src/loader.c $(NEZ_LIB)
	clang src/loader.c -o loader -O$(OPT) -g$(DEBUG)

vm: src/instruction.h src/vm.c src/vm_core.c vmgen.rb
	clang src/vm.c -o vm -O$(OPT) -g$(DEBUG)

src/vm_core.c: vmgen.rb src/instruction.def
	$(RUBY) vmgen.rb src/instruction.def > $@

gen: sample/math.nzc sample/json.nzc

# nez.nzc:
sample/%.nzc: sample/%.nez
	java -jar $(NEZ) compile -p $<

clean:
	-rm sample/*.nzc loader src/vm_core.c

.PHONY: all gen clean
