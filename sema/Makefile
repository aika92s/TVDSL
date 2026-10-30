# HW3: семантические проверки (запускать из папки sema/ или через корневой Makefile: make -C sema ...)
# Лексер (../lexer) и парсер (../parser) берутся только на чтение. Всё сгенерированное — в sema/.
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
LEXDIR   := ../lexer
PARDIR   := ../parser
LIB      := $(LEXDIR)/src/funnylex.cpp
SRCS     := $(PARDIR)/src/ast.cpp $(PARDIR)/src/parser.cpp src/sema.cpp
HDRS     := $(LEXDIR)/src/funnylex.hpp $(PARDIR)/src/ast.hpp $(PARDIR)/src/parser.hpp src/sema.hpp
INC      := -Isrc -I$(PARDIR)/src -I$(LEXDIR)/src
TOKENS   := $(LEXDIR)/data/tokens.txt

all: build/funny_check build/sema_tests

build/funny_check: src/sema_main.cpp $(SRCS) $(LIB) $(HDRS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(INC) -DFUNNY_TOKENS='"$(TOKENS)"' src/sema_main.cpp $(SRCS) $(LIB) -o $@

build/sema_tests: tests/sema_tests.cpp $(SRCS) $(LIB) $(HDRS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(INC) -DFUNNY_TOKENS='"$(TOKENS)"' tests/sema_tests.cpp $(SRCS) $(LIB) -o $@

# пишет sema/REPORT_HW3.md
test: build/sema_tests build/funny_check
	timeout 120 ./build/sema_tests

clean:
	rm -rf build

.PHONY: all test clean
