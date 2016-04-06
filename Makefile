
COMPILER := gcc
COMPILER_FLAGS := -std=c++11 -g
EXENAME := hplangc.exe

SOURCES := \
	src/compiler.cpp \
	src/lexer.cpp \
	src/memory.cpp \
	src/token.cpp

COMPILER_SOURCES := \
	src/main.cpp \
	$(SOURCES)

build:
	$(COMPILER) $(COMPILER_FLAGS) $(COMPILER_SOURCES) -o $(EXENAME)


run:
	@#-./$(EXENAME) samples/fibo.hp
	@#-./$(EXENAME) tests/token_test.hp
	-./$(EXENAME) tests/crlf_test.hp


run_debug:
	$(GDB) ./$(EXENAME) 2> /dev/null


TESTEXE := tests/tests.exe

build_tests:
	$(COMPILER) $(COMPILER_FLAGS) tests/tests.cpp $(SOURCES) -o $(TESTEXE)

run_tests: build_tests
	./$(TESTEXE)

GDB := /usr/bin/gdb.exe

