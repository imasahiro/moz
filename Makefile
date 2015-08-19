# MOZ Makefile
CC=clang
RUBY=ruby
NEZ=../nez.jar
BUILD=build
SRC=src
NEZ_LIB=src/bitset.h src/instruction.h src/pstring.h src/mozvm.h src/ast.h
NEZ_CORE=$(BUILD)/ast.o $(BUILD)/memo.o $(BUILD)/symtable.o $(BUILD)/node.o
OPTION=-O0 -g3 -Wall -I$(SRC)

all: vm test
vm: $(BUILD)/vm.o $(BUILD)/loader.o $(NEZ_CORE) src/main.c
	$(CC) $(OPTION) $? -o $(BUILD)/vm

test: test_ast test_sym test_memo test_loader gen
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

test_ast: $(BUILD)/ast.o $(BUILD)/node.o test/test_ast.c
	$(CC) $(OPTION) $? -o $(BUILD)/test_ast

test_sym: $(BUILD)/symtable.o test/test_sym.c
	$(CC) $(OPTION) $? -o $(BUILD)/test_sym

test_memo: $(BUILD)/node.o $(BUILD)/memo.o test/test_memo.c
	$(CC) $(OPTION) $? -o $(BUILD)/test_memo

test_loader: $(NEZ_CORE) $(BUILD)/loader.o $(BUILD)/vm.o test/test_loader.c
	$(CC) $(OPTION) $? -o $(BUILD)/test_loader

gen: sample/math.nzc sample/json.nzc

# nez.nzc:
sample/%.nzc: sample/%.nez
	java -jar $(NEZ) compile -p $<

clean:
	-rm -rf sample/*.nzc build/* src/vm_core.c src/vm_inst.h

.PHONY: all gen clean
