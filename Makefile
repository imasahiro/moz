
NEZ=../nez.jar
OPT=0
DEBUG=3
NEZ_LIB=src/bitset.h src/instruction.h src/pstring.h src/mozvm.h src/ast.h
RUBY=ruby

all: loader gen ast

loader: src/loader.c $(NEZ_LIB)
	clang src/loader.c -o loader -O$(OPT) -g$(DEBUG)

vm: src/instruction.h src/vm.c src/vm_core.c src/ast.c vmgen.rb $(NEZ_LIB)
	clang src/vm.c src/ast.c -o vm -O$(OPT) -g$(DEBUG)

src/vm_core.c: vmgen.rb src/instruction.def
	$(RUBY) vmgen.rb src/instruction.def > $@

ast: src/ast.c $(NEZ_LIB)
	clang src/ast.c -o vm -O$(OPT) -g$(DEBUG)

map: src/kmap.c src/kmap.h $(NEZ_LIB)
	clang src/kmap.c -c -O$(OPT) -g$(DEBUG) -Wall

gen: sample/math.nzc sample/json.nzc

# nez.nzc:
sample/%.nzc: sample/%.nez
	java -jar $(NEZ) compile -p $<

clean:
	-rm sample/*.nzc loader src/vm_core.c

.PHONY: all gen clean
