#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>

#include "parser.hpp"

using namespace funnyparse;

// Читает файл целиком; "-" означает stdin.
static bool readAll(const std::string& path, std::string& out) {
    if (path == "-") {
        out.assign(std::istreambuf_iterator<char>(std::cin), {});
        return true;
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), {});
    return true;
}

#ifndef FUNNY_TOKENS
#define FUNNY_TOKENS "../lexer/data/tokens.txt"
#endif

// AST печатается в stdout, ошибки — в stderr. Код возврата: 0 — ок, 1 — ошибки разбора, 2 — неверный вызов.
int main(int argc, char** argv) {
    std::string tokensPath = FUNNY_TOKENS, input;
    enum { TREE, JSON, SEXPR } mode = TREE;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--json") mode = JSON;
        else if (a == "--sexpr") mode = SEXPR;
        else if (a == "--tokens" && i + 1 < argc) tokensPath = argv[++i];
        else if (input.empty() && (a == "-" || a[0] != '-')) input = a;
        else { std::cerr << "usage: funny_parse [--tokens F] [--json | --sexpr] <file | ->\n"; return 2; }
    }
    if (input.empty()) { std::cerr << "usage: funny_parse [--tokens F] [--json | --sexpr] <file | ->\n"; return 2; }

    try {
        funnylex::Lexer lexer(funnylex::buildMinimalDFA(funnylex::loadTokenDefs(tokensPath)));
        std::string src;
        if (!readAll(input, src)) { std::cerr << "cannot open " << input << "\n"; return 2; }
        ParseResult r = parseProgram(src, lexer);
        for (const auto& d : r.errors) std::cerr << formatDiagnostic(src, d, input == "-" ? "<stdin>" : input);
        if (!r.ok()) { std::cerr << r.errors.size() << " error(s)\n"; return 1; }
        if (mode == JSON) std::cout << dumpJSON(*r.ast);
        else if (mode == SEXPR) std::cout << dumpSexpr(*r.ast) << "\n";
        else std::cout << dumpTree(*r.ast);
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return 2;
    }
}
