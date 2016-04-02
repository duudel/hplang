
COMPILER := gcc
COMPILER_FLAGS := -std=c++11
EXENAME := hplangc.exe

SOURCES := \
	src/main.cpp \
	src/lexer.cpp \
	src/memory.cpp \
	src/token.cpp

build:
	$(COMPILER) $(COMPILER_FLAGS) $(SOURCES) -o $(EXENAME)

