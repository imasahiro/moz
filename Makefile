# MOZ Makefile
CC=clang
RUBY=ruby
NEZ=../nez.jar
BUILD=build
SRC=src
NEZ_LIB=src/bitset.h src/instruction.h src/pstring.h src/mozvm.h src/ast.h
NEZ_CORE=$(BUILD)/ast.o $(BUILD)/memo.o $(BUILD)/symtable.o $(BUILD)/node.o
OPTION=-march=native -O3 -g3 -Wall -I$(SRC)
# OPTION=-march=native -O0 -g3 -Wall -I$(SRC)
M=
# M=valgrind --leak-check=full --show-leak-kinds=all

all: moz test
moz: $(BUILD)/vm.o $(BUILD)/loader.o $(NEZ_CORE) src/main.c gen
	$(CC) $(OPTION) $(BUILD)/vm.o $(BUILD)/loader.o $(NEZ_CORE) src/main.c -o $(BUILD)/moz

test: test2 test_math test_json

test2: gen test_node test_ast test_sym test_memo test_loader test_inst
	$(M) $(BUILD)/test_inst
	$(M) $(BUILD)/test_node
	$(M) $(BUILD)/test_ast
	$(M) $(BUILD)/test_sym
	$(M) $(BUILD)/test_memo
	$(M) $(BUILD)/test_loader sample/math.nzc

src/vm_core.c: tool/vmgen.rb src/instruction.def src/instruction.h
	$(RUBY) tool/vmgen.rb src/instruction.def > $@

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

test_inst: test/test_inst.c
	$(CC) $(OPTION) $? -o $(BUILD)/test_inst

test_loader: src/vm_core.c $(NEZ_CORE) $(BUILD)/loader.o $(BUILD)/vm.o test/test_loader.c
	$(CC) $(OPTION)  $(NEZ_CORE) $(BUILD)/loader.o $(BUILD)/vm.o test/test_loader.c -o $(BUILD)/test_loader

test_math: moz gen
	$(M) $(BUILD)/moz -p sample/math.nzc -i sample/sample.math
	$(M) $(BUILD)/moz -p sample/math.nzc -i sample/sample2.math

test_json: moz gen
	$(M) $(BUILD)/moz -p sample/json.nzc -i sample/sample.json
	$(M) $(BUILD)/moz -p sample/json.nzc -i sample/sample2.json
	$(M) $(BUILD)/moz -p sample/json.nzc -i sample/sample3.json

gen: sample/math.nzc sample/json.nzc sample/xml.nzc

# nez.nzc:
sample/%.nzc: sample/%.nez
	# java -jar $(NEZ) compile -p $<
	java -jar $(NEZ) compile --option:-ast -p $<

clean:
	-rm -rf sample/*.nzc build/* src/vm_core.c src/vm_inst.h

.PHONY: all gen clean
