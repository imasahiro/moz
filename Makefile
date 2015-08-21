# MOZ Makefile
CC=clang
RUBY=ruby
NEZ=../nez.jar
BUILD=build
SRC=src
NEZ_LIB=src/bitset.h src/instruction.h src/pstring.h src/mozvm.h src/ast.h
NEZ_CORE=$(BUILD)/ast.o $(BUILD)/memo.o $(BUILD)/symtable.o $(BUILD)/node.o
OPTION=-O0 -g3 -Wall -I$(SRC)

all: moz test
moz: $(BUILD)/vm.o $(BUILD)/loader.o $(NEZ_CORE) src/main.c gen
	$(CC) $(OPTION) $(BUILD)/vm.o $(BUILD)/loader.o $(NEZ_CORE) src/main.c -o $(BUILD)/moz

test: test2 test_math test_json

test2: gen test_node test_ast test_sym test_memo test_loader
	$(BUILD)/test_node
	$(BUILD)/test_ast
	$(BUILD)/test_sym
	$(BUILD)/test_memo
	$(BUILD)/test_loader sample/math.nzc

src/vm_core.c: vmgen.rb src/instruction.def src/instruction.h
	$(RUBY) vmgen.rb src/instruction.def > $@

$(BUILD)/ast.o: src/ast.c src/ast.h $(NEZ_LIB)
	$(CC) $(OPTION) src/ast.c -c -o $@

$(BUILD)/memo.o: src/memo.c src/memo.h $(NEZ_LIB)
	$(CC) $(OPTION) src/memo.c -c -o $@

$(BUILD)/symtable.o: src/symtable.c src/symtable.h $(NEZ_LIB)
	$(CC) $(OPTION) src/symtable.c -c -o $@

$(BUILD)/node.o: src/node.c src/node.h $(NEZ_LIB)
	$(CC) $(OPTION) src/node.c -c -o $@

$(BUILD)/vm.o: src/vm.c src/node.h src/ast.h src/symtable.h src/vm_core.c $(NEZ_LIB)
	$(CC) $(OPTION) src/vm.c -c -o $@

$(BUILD)/loader.o: src/loader.c src/node.h src/ast.h src/symtable.h $(NEZ_LIB)
	$(CC) $(OPTION) src/loader.c -c -o $@

test_node: $(BUILD)/node.o test/test_node.c
	$(CC) $(OPTION) $? -o $(BUILD)/test_node

test_ast: $(BUILD)/ast.o $(BUILD)/node.o test/test_ast.c
	$(CC) $(OPTION) $? -o $(BUILD)/test_ast

test_sym: $(BUILD)/symtable.o test/test_sym.c
	$(CC) $(OPTION) $? -o $(BUILD)/test_sym

test_memo: $(BUILD)/node.o $(BUILD)/memo.o test/test_memo.c
	$(CC) $(OPTION) $? -o $(BUILD)/test_memo

test_loader:
	$(CC) $(OPTION) $(NEZ_CORE) $(BUILD)/loader.o $(BUILD)/vm.o test/test_loader.c$? -o $(BUILD)/test_loader

test_math: moz gen
	$(BUILD)/moz -p sample/math.nzc -i sample/sample.math
	$(BUILD)/moz -p sample/math.nzc -i sample/sample2.math

test_json: moz gen
	$(BUILD)/moz -p sample/json.nzc -i sample/sample.json
	$(BUILD)/moz -p sample/json.nzc -i sample/sample2.json
	$(BUILD)/moz -p sample/json.nzc -i sample/sample3.json

gen: sample/math.nzc sample/json.nzc

# nez.nzc:
sample/%.nzc: sample/%.nez
	java -jar $(NEZ) compile -p $<

clean:
	-rm -rf sample/*.nzc build/* src/vm_core.c src/vm_inst.h

.PHONY: all gen clean
