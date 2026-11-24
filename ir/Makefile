# HW4: IR, CFG, локальные оптимизации, живость и раскраска.
# HW1-HW3 подключаются только на чтение; все результаты остаются в ir/.
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -pedantic
LEXDIR   := ../lexer
PARDIR   := ../parser
SEMADIR  := ../sema
INC      := -Isrc -I$(SEMADIR)/src -I$(PARDIR)/src -I$(LEXDIR)/src
TOKENS   := $(LEXDIR)/data/tokens.txt
FRONTEND := $(LEXDIR)/src/funnylex.cpp $(PARDIR)/src/ast.cpp $(PARDIR)/src/parser.cpp $(SEMADIR)/src/sema.cpp
IRSRCS   := src/ir.cpp src/generator.cpp src/analysis.cpp src/optimizer.cpp src/interpreter.cpp
HDRS     := src/ir.hpp src/generator.hpp src/analysis.hpp src/optimizer.hpp src/interpreter.hpp

all: build/funny_ir build/ir_tests

build/funny_ir: src/main.cpp $(IRSRCS) $(FRONTEND) $(HDRS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(INC) -DFUNNY_TOKENS='"$(TOKENS)"' src/main.cpp $(IRSRCS) $(FRONTEND) -o $@

build/ir_tests: tests/ir_tests.cpp $(IRSRCS) $(FRONTEND) $(HDRS)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(INC) -DFUNNY_TOKENS='"$(TOKENS)"' tests/ir_tests.cpp $(IRSRCS) $(FRONTEND) -o $@

test: build/ir_tests build/funny_ir
	timeout 120 ./build/ir_tests
	./build/funny_ir --format json examples/optimization.fn > /dev/null

examples: build/funny_ir
	./build/funny_ir --function compute --cfg-dot examples/cfg.dot --interference-dot examples/interference.dot examples/optimization.fn > examples/output.txt

clean:
	rm -rf build

.PHONY: all test examples clean
