# MOZ Makefile
NEZ=../nez.jar
NEZ_LIB=src/bitset.h src/instruction.h src/pstring.h src/mozvm.h src/ast.h
NEZ_CORE=src/ast.c src/memo.c src/symtable.c
RUBY=ruby
OPTION=-O0 -g3 -Wall
BUILD=build

all: loader vm ast memo sym

loader: src/loader.c $(NEZ_LIB)
	clang src/loader.c -o $(BUILD)/loader $(OPTION)

vm: src/instruction.h src/vm.c src/vm_core.c $(NEZ_CORE) $(NEZ_LIB)
	clang src/vm.c src/loader.c $(NEZ_CORE) -o $(BUILD)/vm $(OPTION)

src/vm_core.c: vmgen.rb src/instruction.def
	$(RUBY) vmgen.rb src/instruction.def > $@

ast: src/ast.c $(NEZ_LIB)
	clang src/ast.c -o $(BUILD)/ast $(OPTION) -DDEBUG=1

sym: src/symtable.c $(NEZ_LIB)
	clang src/symtable.c -o $(BUILD)/sym $(OPTION) -DDEBUG=1

memo: src/memo.c $(NEZ_LIB)
	clang src/memo.c -o $(BUILD)/memo $(OPTION) -DDEBUG=1

map: src/kmap.c src/kmap.h $(NEZ_LIB)
	clang src/kmap.c -c $(OPTION)

gen: sample/math.nzc sample/json.nzc

# nez.nzc:
sample/%.nzc: sample/%.nez
	java -jar $(NEZ) compile -p $<

clean:
	-rm sample/*.nzc loader src/vm_core.c

.PHONY: all gen clean
