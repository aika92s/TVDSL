#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>

#include "funnylex.hpp"

using namespace funnylex;

static std::string show(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        if (c == '\n') out += "\\n";
        else if (c == '\t') out += "\\t";
        else if (c == '\r') out += "\\r";
        else if (c == '|') out += "\\|";
        else if (c < 0x20 || c > 0x7e) { char b[8]; std::snprintf(b, sizeof b, "\\x%02x", c); out += b; }
        else out += static_cast<char>(c);
    }
    return out;
}

int main(int argc, char** argv) {
    std::string tokensPath = "data/tokens.txt", outDir = "output", testsPath = "data/tests.jsonl",
                reportPath = "REPORT.md";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (i + 1 >= argc) { std::cerr << "usage: funny_lexgen [--tokens F] [--out-dir D] [--tests F] [--report F]\n"; return 2; }
        std::string v = argv[++i];
        if (a == "--tokens") tokensPath = v;
        else if (a == "--out-dir") outDir = v;
        else if (a == "--tests") testsPath = v;
        else if (a == "--report") reportPath = v;
        else { std::cerr << "unknown option: " << a << "\n"; return 2; }
    }

    try {
        auto defs = loadTokenDefs(tokensPath);
        NFA nfa = buildNFA(defs);
        DFA partial = subsetConstruction(nfa, defs);
        DFA complete = completeWithTrap(partial);
        DFA min = minimize(partial);

        std::cout << "tokens: " << defs.size() << "\nNFA: " << nfa.states.size()
                  << "\nDFA (subset construction): " << partial.states.size()
                  << "\nDFA before minimization (+trap): " << complete.states.size()
                  << "\nminimal DFA (Hopcroft, incl. trap): " << min.states.size()
                  << "\nstart state " << min.start << ", trap state " << min.trap << "\n";

        std::filesystem::create_directories(outDir);
        exportJSON(min, outDir + "/dfa_table.json");
        exportCSV(min, outDir + "/dfa_table.csv");
        exportHeader(min, outDir + "/dfa_table.hpp");

        Lexer lexer(min);
        auto tests = loadTestCases(testsPath);
        int passed = 0, good = 0, bad = 0;
        std::string rows;
        for (const auto& t : tests) {
            std::string actual = runTestCase(lexer, t);
            bool ok = actual == t.expected;
            passed += ok;
            (t.bad() ? bad : good)++;
            std::cout << (ok ? "[PASS] " : "[FAIL] ") << (t.bad() ? "bad  " : "good ") << t.name << "\n";
            if (!ok) std::cout << "    input:    " << show(t.input) << "\n    expected: " << show(t.expected)
                               << "\n    actual:   " << show(actual) << "\n";
            rows += "| " + t.name + " | " + (t.bad() ? "bad" : "good") + " | `" + show(t.input) + "` | `" +
                    show(t.expected) + "` | " + (ok ? "PASS" : "FAIL") + " |\n";
        }
        std::cout << passed << "/" << tests.size() << " tests passed (good: " << good << ", bad: " << bad << ")\n";

        std::ofstream rep(reportPath);
        rep << "# HW1 report\n\n## Automaton sizes\n\n| Stage | States |\n|---|---|\n"
            << "| NFA (Thompson) | " << nfa.states.size() << " |\n"
            << "| DFA (subset construction, partial) | " << partial.states.size() << " |\n"
            << "| DFA before minimization (+ trap) | " << complete.states.size() << " |\n"
            << "| Minimal DFA (Hopcroft, incl. trap) | " << min.states.size() << " |\n\n"
            << "Start state: " << min.start << ". Trap state: " << min.trap
            << " (non-accepting, loops to itself on all 128 symbols; bytes >= 128 go there too).\n\n"
            << "## Tokens (" << defs.size() << ")\n\n| priority | name | skip | regex |\n|---|---|---|---|\n";
        for (const auto& d : defs)
            rep << "| " << d.priority << " | " << d.name << " | " << (d.skip ? "yes" : "") << " | `" << show(d.regex) << "` |\n";
        rep << "\n## Tests: " << passed << "/" << tests.size() << " passed (good " << good << ", bad " << bad
            << ")\n\n| name | kind | input | expected | result |\n|---|---|---|---|---|\n" << rows;

        return passed == static_cast<int>(tests.size()) ? 0 : 1;
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return 1;
    }
}
