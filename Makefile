
COMPILER := gcc
COMPILER_FLAGS := -std=c++11 -Wall -Wextra -fno-exceptions -fno-rtti -g
EXENAME := hplangc.exe

SOURCES := \
	src/assert.cpp \
	src/ast_types.cpp \
	src/compiler.cpp \
	src/error.cpp \
	src/hplang.cpp \
	src/io.cpp \
	src/ir_gen.cpp \
	src/lexer.cpp \
	src/memory.cpp \
	src/compiler_options.cpp \
	src/parser.cpp \
	src/semantic_check.cpp \
	src/symbols.cpp \
	src/token.cpp

COMPILER_SOURCES := \
	src/main.cpp \
	$(SOURCES)

build:
	$(COMPILER) $(COMPILER_FLAGS) $(COMPILER_SOURCES) -o $(EXENAME)


run:
	@#-./$(EXENAME) samples/compiletime.hp
	@#-./$(EXENAME) samples/fibo.hp
	@#-./$(EXENAME) samples/beer.hp
	-./$(EXENAME) samples/factorial.hp


GDB := /usr/bin/gdb.exe


run_debug:
	$(GDB) ./$(EXENAME) 2> /dev/null


TESTEXE := tests/tests.exe

build_tests:
	$(COMPILER) $(COMPILER_FLAGS) tests/tests.cpp $(SOURCES) -o $(TESTEXE)

run_tests: build_tests
	./$(TESTEXE)
	@#$(GDB) ./$(TESTEXE)

