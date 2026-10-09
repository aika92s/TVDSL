# HW2: парсер (запускать из папки parser/ или через корневой Makefile: make -C parser ...)
# Лексер берётся из ../lexer только на чтение: его исходники и data/tokens.txt.
# Парсер ничего не пишет в lexer/ (ни REPORT.md, ни output/). Всё сгенерированное — в parser/.
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
LEXDIR   := ../lexer
LIB      := $(LEXDIR)/src/funnylex.cpp
SRCS     := src/ast.cpp src/parser.cpp
HDRS     := $(LEXDIR)/src/funnylex.hpp src/ast.hpp src/parser.hpp
INC      := -Isrc -I$(LEXDIR)/src
TOKENS   := $(LEXDIR)/data/tokens.txt

all: build/funny_parse build/parser_tests

build/funny_parse: src/parse_main.cpp $(SRCS) $(LIB) $(HDRS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(INC) -DFUNNY_TOKENS='"$(TOKENS)"' src/parse_main.cpp $(SRCS) $(LIB) -o $@

build/parser_tests: tests/parser_tests.cpp $(SRCS) $(LIB) $(HDRS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(INC) -DFUNNY_TOKENS='"$(TOKENS)"' tests/parser_tests.cpp $(SRCS) $(LIB) -o $@

# пишет parser/REPORT_HW2.md
test: build/parser_tests build/funny_parse
	timeout 120 ./build/parser_tests

clean:
	rm -rf build

.PHONY: all test clean
