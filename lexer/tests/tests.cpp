// Тесты без внешних зависимостей. Запуск из корня проекта: make test
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>

#include "funnylex.hpp"

using namespace funnylex;

static int total = 0, failed = 0;
#define CHECK(cond) do { ++total; if (!(cond)) { ++failed; std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; } } while (0)
#define CHECK_EQ(a, b) do { ++total; auto va = (a); auto vb = (b); if (!(va == vb)) { ++failed; \
    std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << ": " #a " == " #b "\n    got:      " << va << "\n    expected: " << vb << "\n"; } } while (0)

static const char* kTokens = "data/tokens.txt";
static const char* kTests = "data/tests.jsonl";

// ДКА для одной регулярки (один токен T).
static Lexer single(const std::string& regex) { return Lexer(buildMinimalDFA({{"T", regex, false, 0}})); }
static bool matches(const std::string& regex, const std::string& s) { return single(regex).acceptsWhole(s); }
static bool throwsOnBuild(const std::string& regex) {
    try { buildNFA({{"T", regex, false, 0}}); } catch (const std::runtime_error&) { return true; }
    return false;
}
static std::string lex(const Lexer& l, const std::string& s) { return describeTokens(l.tokenize(s)); }

// Независимая проверка минимальности: алгоритм Мура, число классов эквивалентности достижимых состояний.
static int mooreClasses(const DFA& d0) {
    DFA d = completeWithTrap(d0);
    std::vector<bool> seen(d.states.size(), false);
    std::vector<int> order{d.start};
    seen[d.start] = true;
    for (size_t i = 0; i < order.size(); ++i)
        for (int t : d.states[order[i]].trans)
            if (!seen[t]) { seen[t] = true; order.push_back(t); }
    std::map<int, int> cls;  // состояние -> класс
    for (int s : order) cls[s] = d.states[s].accept;
    size_t count = 0;
    while (true) {
        std::map<std::vector<int>, int> sig;
        std::map<int, int> next;
        for (int s : order) {
            std::vector<int> key{cls[s]};
            for (int t : d.states[s].trans) key.push_back(cls[t]);
            next[s] = sig.emplace(key, static_cast<int>(sig.size())).first->second;
        }
        cls = next;
        if (sig.size() == count) return static_cast<int>(count);
        count = sig.size();
    }
}

static std::vector<TokenDef> funnyTokens() { return loadTokenDefs(kTokens); }

// регулярки и Томпсон

static void test_regex_dialect() {
    CHECK(matches("abc", "abc") && !matches("abc", "ab") && !matches("abc", "abcd"));
    CHECK(matches("a|b", "a") && matches("a|b", "b") && !matches("a|b", "ab"));
    CHECK(matches("ba*", "b") && matches("ba*", "baaa") && !matches("ba*", "a"));
    CHECK(matches("ba+", "ba") && !matches("ba+", "b"));
    CHECK(matches("ba?", "b") && matches("ba?", "ba") && !matches("ba?", "baa"));
    CHECK(matches("(ab)+c", "ababc") && !matches("(ab)+c", "aba"));
    CHECK(matches("[a-c]x", "bx") && !matches("[a-c]x", "dx"));
    CHECK(matches("[a-cx-z]", "y") && !matches("[a-cx-z]", "m"));
    CHECK(matches("[^\\n]x", "qx") && !matches("[^\\n]x", "\nx"));
    CHECK(matches("[a-]", "-") && matches("[a-]", "a"));
    CHECK(matches("a.b", "a#b") && !matches("a.b", "ab"));
    CHECK(matches("\\(\\)\\[\\]\\.\\*\\+\\?\\|\\\\", "()[].*+?|\\"));
    CHECK(matches("\\t\\r\\n", "\t\r\n"));
    CHECK(matches("{},;:-/", "{},;:-/"));  // эти символы в регулярке обычные
    CHECK(matches("a(b|)c", "ac") && matches("a(b|)c", "abc"));
}

static void test_regex_errors() {
    for (const char* bad : {"(", "(a", "a)", "[", "[a", "[]", "[z-a]", "*a", "+", "?", "a|*", "\\", "]"})
        CHECK(throwsOnBuild(bad));
    CHECK(!throwsOnBuild("a"));
}

static void test_thompson_shape() {
    NFA a = buildNFA({{"T", "a", false, 0}});
    CHECK_EQ(a.states.size(), size_t(3));  // старт + 2 состояния фрагмента
    int accepting = 0;
    for (const auto& s : a.states) accepting += s.accept != 0;
    CHECK_EQ(accepting, 1);
}

//  НКА -> ДКА, приоритеты

static void test_priority() {
    std::vector<TokenDef> defs = {{"WS", " +", true, 0}, {"KW", "if", false, 1}, {"ID", "[a-z]+", false, 2}};
    Lexer l(buildMinimalDFA(defs));
    CHECK_EQ(lex(l, "if iff i"), std::string("KW(if) ID(iff) ID(i)"));
    std::vector<TokenDef> swapped = {{"ID", "[a-z]+", false, 0}, {"KW", "if", false, 1}};
    CHECK_EQ(lex(Lexer(buildMinimalDFA(swapped)), "if"), std::string("ID(if)"));
}

static void test_subset_is_partial_without_trap() {
    auto defs = std::vector<TokenDef>{{"T", "ab", false, 0}};
    DFA d = subsetConstruction(buildNFA(defs), defs);
    CHECK_EQ(d.trap, -1);
    CHECK_EQ(d.states.size(), size_t(3));  // старт, после a, после ab
    CHECK_EQ(d.states[d.start].trans['a'] != -1, true);
    CHECK_EQ(d.states[d.start].trans['b'], -1);
}

// ------------------------------------------------------------------ ловушка

static void check_trap(const DFA& d) {
    CHECK(d.trap >= 0);
    const DFAState& t = d.states[d.trap];
    CHECK_EQ(t.accept, 0);
    bool selfLoops = true;
    for (int x : t.trans) selfLoops &= (x == d.trap);
    CHECK(selfLoops);
    bool complete = true;
    for (const auto& s : d.states) for (int x : s.trans) complete &= (x >= 0 && x < int(d.states.size()));
    CHECK(complete);  // таблица полная: нет переходов -1
}

static void test_trap() {
    auto defs = funnyTokens();
    auto partial = subsetConstruction(buildNFA(defs), defs);
    DFA complete = completeWithTrap(partial);
    CHECK_EQ(complete.states.size(), partial.states.size() + 1);
    check_trap(complete);
    check_trap(minimize(partial));
    check_trap(minimize(complete));
    Lexer l(buildMinimalDFA(defs));
    int trap = l.dfa().trap;
    for (int b = 128; b < 256; ++b) CHECK_EQ(l.run(std::string(1, char(b))), trap);   // non-ASCII -> ловушка
    CHECK_EQ(l.run("x\xE9y"), trap);                                                 // и посреди строки
    CHECK_EQ(l.run("00"), trap);                                                     // 00 — тупик
    CHECK(l.run("!") != trap && !l.acceptsWhole("!"));                               // "!" — только префикс "!="
    CHECK_EQ(l.run("!!"), trap);                                                     // ловушка поглощающая
    CHECK_EQ(l.run("00zzz"), trap);
}

// минимизация

static void test_minimize_classic() {
    DFA d = buildMinimalDFA({{"T", "(a|b)*abb", false, 0}});
    CHECK_EQ(d.states.size(), size_t(5));  // классические 4 состояния + ловушка
    CHECK_EQ(mooreClasses(d), 5);
}

static void test_minimize_funny() {
    auto defs = funnyTokens();
    auto partial = subsetConstruction(buildNFA(defs), defs);
    DFA complete = completeWithTrap(partial);
    DFA min = minimize(partial);
    CHECK(min.states.size() < complete.states.size());
    CHECK_EQ(int(min.states.size()), mooreClasses(partial));   // Хопкрофт == Мур
    CHECK_EQ(minimize(min).states.size(), min.states.size());  // идемпотентность
    // склейки не нарушают принимаемые токены: у каждого токена остался принимающий класс
    std::set<int> acc;
    for (const auto& s : min.states) if (s.accept) acc.insert(s.accept);
    CHECK_EQ(acc.size(), defs.size());
}

static void test_minimized_equals_original_on_random_strings() {
    auto defs = funnyTokens();
    DFA complete = completeWithTrap(subsetConstruction(buildNFA(defs), defs));
    Lexer orig(complete), min(minimize(complete));
    const std::string chars = "abfiexl0159 \t\r\n/=<>!-+*()[]{},;:|_#\xE9 uwhnrtcsvd";
    srand(12345);
    int diff = 0;
    for (int n = 0; n < 3000; ++n) {
        std::string s;
        for (int i = rand() % 24; i > 0; --i) s += chars[rand() % chars.size()];
        diff += lex(orig, s) != lex(min, s);
    }
    CHECK_EQ(diff, 0);
}

// ------------------------------------------------------------------ лексер на таблице Funny

static void test_funny_lexer() {
    Lexer l(buildMinimalDFA(funnyTokens()));
    // Пустая строка и пробелы
    CHECK_EQ(lex(l, ""), std::string(""));
    CHECK_EQ(lex(l, " \t\r\n"), std::string(""));
    // INT: 0 принимается; 00 и 01 — нет
    std::string t;
    CHECK(l.acceptsWhole("0", &t) && t == "INT");
    CHECK(l.acceptsWhole("90210", &t) && t == "INT");
    CHECK(!l.acceptsWhole("00") && !l.acceptsWhole("01"));
    CHECK_EQ(lex(l, "01"), std::string("INT(0) INT(1)"));
    // обязательные ключевые слова задания
    for (const char* kw : {"function", "returns", "while", "if", "else", "assert", "assume", "invariant", "length"}) {
        CHECK(l.acceptsWhole(kw, &t));
        CHECK_EQ(t, "KW_" + [&] { std::string u = kw; for (auto& c : u) c = char(toupper(c)); return u; }());
    }
    CHECK(l.acceptsWhole("formula", &t) && t == "IDENT");  // в Funny formula — обычное имя
    // идентификатор с подчёркиванием: по спецификации Funny `_` недопустим -> ловушка
    CHECK_EQ(lex(l, "a_b"), std::string("IDENT(a) ERR(_) IDENT(b)"));
    // комментарий до конца строки
    CHECK_EQ(lex(l, "a//b\nc"), std::string("IDENT(a) IDENT(c)"));
    // позиции токенов
    auto toks = l.tokenize("ab  =1");
    CHECK_EQ(toks.size(), size_t(3));
    CHECK_EQ(toks[0].pos, size_t(0));
    CHECK_EQ(toks[1].pos, size_t(4));
    CHECK_EQ(toks[2].pos, size_t(5));
}

static void test_underscore_variant() {  // если задание всё же требует `_` в именах: правится одна регулярка
    auto defs = funnyTokens();
    for (auto& d : defs) if (d.name == "IDENT") d.regex = "[a-zA-Z_][a-zA-Z0-9_]*";
    Lexer l(buildMinimalDFA(defs));
    CHECK_EQ(lex(l, "my_var _x if_"), std::string("IDENT(my_var) IDENT(_x) IDENT(if_)"));
}

static void test_token_file() {
    auto defs = funnyTokens();
    CHECK(defs.size() > 40);
    CHECK_EQ(defs[0].name, std::string("WS"));
    CHECK(defs[0].skip && defs[1].skip && !defs[2].skip);
    std::ofstream("/tmp/funnylex_bad_tokens.txt") << "A TOKEN (\n";
    bool threw = false;
    try { buildNFA(loadTokenDefs("/tmp/funnylex_bad_tokens.txt")); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
    threw = false;
    try { loadTokenDefs("/nonexistent/tokens.txt"); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);
}

// ------------------------------------------------------------------ данные из tests.jsonl

static void test_data_file() {
    auto defs = funnyTokens();
    DFA complete = completeWithTrap(subsetConstruction(buildNFA(defs), defs));
    Lexer min(minimize(complete)), orig(complete);
    auto tests = loadTestCases(kTests);
    CHECK(tests.size() >= 60);
    int good = 0, bad = 0;
    for (const auto& tc : tests) {
        CHECK_EQ(runTestCase(min, tc), tc.expected);
        CHECK_EQ(runTestCase(orig, tc), tc.expected);  // исходный ДКА даёт то же самое
        (tc.bad() ? bad : good)++;
        // имя отражает вид теста
        CHECK_EQ(tc.name.rfind(tc.bad() ? "bad_" : "good_", 0), size_t(0));
    }
    CHECK(good >= 30 && bad >= 15);
}

// экспорт

static void test_export_csv() {
    DFA d = buildMinimalDFA(funnyTokens());
    exportCSV(d, "/tmp/funnylex_table.csv");
    std::ifstream in("/tmp/funnylex_table.csv");
    std::string line;
    std::getline(in, line);  // заголовок
    size_t row = 0;
    bool same = true;
    while (std::getline(in, line)) {
        std::stringstream ss(line);
        std::string cell;
        std::vector<std::string> cells;
        while (std::getline(ss, cell, ',')) cells.push_back(cell);
        same &= cells.size() == 5 + kAlphabet && cells[1] == d.names[d.states[row].accept];
        for (int c = 0; same && c < kAlphabet; ++c) same &= std::stoi(cells[5 + c]) == d.states[row].trans[c];
        same &= (cells[4] == "1") == (int(row) == d.trap);
        ++row;
    }
    CHECK(same);
    CHECK_EQ(row, d.states.size());
}

static void test_export_files_written() {
    DFA d = buildMinimalDFA(funnyTokens());
    exportJSON(d, "/tmp/funnylex_table.json");
    exportHeader(d, "/tmp/funnylex_table.hpp");
    std::ifstream j("/tmp/funnylex_table.json"), h("/tmp/funnylex_table.hpp");
    std::string js((std::istreambuf_iterator<char>(j)), {}), hs((std::istreambuf_iterator<char>(h)), {});
    CHECK(js.find("\"trap_state\": " + std::to_string(d.trap)) != std::string::npos);
    CHECK(hs.find("kTrapState = " + std::to_string(d.trap)) != std::string::npos);
    CHECK(hs.find("TOK_IDENT") != std::string::npos);
}

int main() {
    struct { const char* name; void (*fn)(); } all[] = {
        {"regex dialect", test_regex_dialect},
        {"regex errors", test_regex_errors},
        {"thompson shape", test_thompson_shape},
        {"token priority", test_priority},
        {"subset construction", test_subset_is_partial_without_trap},
        {"trap", test_trap},
        {"minimize classic", test_minimize_classic},
        {"minimize funny", test_minimize_funny},
        {"minimized == original (random)", test_minimized_equals_original_on_random_strings},
        {"funny lexer", test_funny_lexer},
        {"underscore variant", test_underscore_variant},
        {"token file", test_token_file},
        {"data/tests.jsonl", test_data_file},
        {"export csv", test_export_csv},
        {"export json/hpp", test_export_files_written},
    };
    for (auto& t : all) {
        int before = failed;
        try { t.fn(); } catch (const std::exception& ex) { ++failed; std::cerr << "  EXCEPTION: " << ex.what() << "\n"; }
        std::cout << (failed == before ? "[ OK ] " : "[FAIL] ") << t.name << "\n";
    }
    std::cout << (total - failed) << "/" << total << " checks passed\n";
    return failed ? 1 : 0;
}
