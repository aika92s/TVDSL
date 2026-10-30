#include <fstream>
#include <iostream>
#include <iterator>

#include "sema.hpp"

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

// Аннотированный AST печатается в stdout, ошибки (синтаксические и семантические) — в stderr.
// Код возврата: 0 — ок, 1 — есть ошибки, 2 — неверный вызов.
// При синтаксических ошибках AST не печатается (проверка типов идёт только по корректно разобранной программе).
int main(int argc, char** argv) {
    std::string tokensPath = FUNNY_TOKENS, input;
    enum { TREE, JSON, SEXPR } mode = TREE;
    const char* usage = "usage: funny_check [--tokens F] [--json | --sexpr] <file | ->\n";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--json") mode = JSON;
        else if (a == "--sexpr") mode = SEXPR;
        else if (a == "--tokens" && i + 1 < argc) tokensPath = argv[++i];
        else if (input.empty() && (a == "-" || a[0] != '-')) input = a;
        else { std::cerr << usage; return 2; }
    }
    if (input.empty()) { std::cerr << usage; return 2; }

    try {
        funnylex::Lexer lexer(funnylex::buildMinimalDFA(funnylex::loadTokenDefs(tokensPath)));
        std::string src;
        if (!readAll(input, src)) { std::cerr << "cannot open " << input << "\n"; return 2; }
        const std::string file = input == "-" ? "<stdin>" : input;

        ParseResult pr = parseProgram(src, lexer);
        if (!pr.ok()) {
            for (const auto& d : pr.errors) std::cerr << formatDiagnostic(src, d, file);
            std::cerr << pr.errors.size() << " syntax error(s)\n";
            return 1;
        }
        funnysema::SemaResult sr = funnysema::analyze(*pr.ast);
        for (const auto& d : sr.errors) std::cerr << formatDiagnostic(src, d, file);
        if (mode == JSON) std::cout << funnysema::dumpAnnotatedJSON(*pr.ast, sr);
        else if (mode == SEXPR) std::cout << funnysema::dumpAnnotatedSexpr(*pr.ast, sr) << "\n";
        else std::cout << funnysema::dumpAnnotatedTree(*pr.ast, sr);
        if (!sr.ok()) { std::cerr << sr.errors.size() << " semantic error(s)\n"; return 1; }
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return 2;
    }
}
