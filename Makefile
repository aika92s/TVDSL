CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
LIB      := src/funnylex.cpp

all: build/funny_lexgen build/standalone_lexer build/tests

build/funny_lexgen: src/main.cpp $(LIB) src/funnylex.hpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -Isrc src/main.cpp $(LIB) -o $@

# таблица переходов: output/dfa_table.{json,csv,hpp} + REPORT.md + прогон data/tests.jsonl
gen: build/funny_lexgen
	./build/funny_lexgen

build/standalone_lexer: examples/standalone_lexer.cpp output/dfa_table.hpp
	$(CXX) $(CXXFLAGS) -Ioutput $< -o $@

build/tests: tests/tests.cpp $(LIB) src/funnylex.hpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -Isrc tests/tests.cpp $(LIB) -o $@

output/dfa_table.hpp: build/funny_lexgen data/tokens.txt
	./build/funny_lexgen > /dev/null

test: build/tests gen build/standalone_lexer
	./build/tests
	./build/standalone_lexer 'if x<=10 { y = 0; } // ok' > /dev/null

clean:
	rm -rf build

.PHONY: all gen test clean
