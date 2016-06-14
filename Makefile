
COMPILER := gcc
COMPILER_FLAGS := -std=c++11 -Wall -Wextra -fno-exceptions -fno-rtti -g
EXENAME := hplangc

SOURCES := \
	src/amd64_codegen.cpp \
	src/args_util.cpp \
	src/assert.cpp \
	src/ast_types.cpp \
	src/codegen.cpp \
	src/compiler.cpp \
	src/error.cpp \
	src/hplang.cpp \
	src/io.cpp \
	src/ir_gen.cpp \
	src/lexer.cpp \
	src/memory.cpp \
	src/compiler_options.cpp \
	src/parser.cpp \
	src/reg_alloc.cpp \
	src/semantic_check.cpp \
	src/symbols.cpp \
	src/token.cpp

COMPILER_SOURCES := \
	src/main.cpp \
	$(SOURCES)

build:
	$(COMPILER) $(COMPILER_FLAGS) $(COMPILER_SOURCES) -o $(EXENAME)


run:
	@#-./$(EXENAME) -di -o samples/fibo samples/fibo.hp 2> stderr.out
	@#-./$(EXENAME) -diR -o samples/beer samples/beer.hp 2> stderr.out
	@#-./$(EXENAME) -diR -o samples/factorial samples/factorial.hp 2> stderr.out
	@#-./$(EXENAME) -o samples/hello samples/hello.hp
	@#-./$(EXENAME) -di tests/member_access.hp 2> stderr.out
	@#-./$(EXENAME) -diR tests/pointer_arith.hp 2> stderr.out
	@#-./$(EXENAME) -dR tests/difficult_rt_infer.hp 2> stderr.out
	-./$(EXENAME) -diR -o out tests/exec/arg_passing.hp 2> stderr.out
	@#-./$(EXENAME) -diR samples/nbody.hp 2> stderr.out
	@#-./$(EXENAME) -diR -o out tests/exec/nbody.hp 2> stderr.out
	@#-./$(EXENAME) -diR tests/exec/reg_pressure.hp 2> stderr.out
	@#-./$(EXENAME) -dRi samples/simple.hp 2> stderr.out
	@#-./$(EXENAME) -dRi samples/test.hp 2> stderr.out
	@#-./$(EXENAME) -dRi -o out samples/fibo.hp 2> stderr.out


GDB := /usr/bin/gdb


run_debug:
	$(GDB) ./$(EXENAME) 2> /dev/null


TESTEXE := tests/tests

build_tests:
	$(COMPILER) $(COMPILER_FLAGS) tests/tests.cpp $(SOURCES) -o $(TESTEXE)

run_tests: build_tests
	./$(TESTEXE)
	@#$(GDB) ./$(TESTEXE)

clean_tests:
	find ./tests -perm /111 -type f -exec rm -v {} \;


