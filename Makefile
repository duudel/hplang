
COMPILER := gcc
COMPILER_FLAGS := -std=c++11 -g
EXENAME := hplangc.exe

SOURCES := \
	src/main.cpp \
	src/lexer.cpp \
	src/memory.cpp \
	src/token.cpp

build:
	$(COMPILER) $(COMPILER_FLAGS) $(SOURCES) -o $(EXENAME)


run:
	-./$(EXENAME) samples/fibo.hp


GDB := /usr/bin/gdb.exe

run_debug:
	$(GDB) ./$(EXENAME)
