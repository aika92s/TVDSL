#pragma once
#include <array>
#include <string>
#include <utility>
#include <vector>

namespace funnylex {

constexpr int kAlphabet = 128;

// Строка таблицы токенов: имя, регулярное выражение, пропускать ли (WS/COMMENT).
// priority = номер строки в файле; при совпадении одной длины побеждает меньший.
struct TokenDef {
    std::string name, regex;
    bool skip = false;
    int priority = 0;
};

struct NFAState {
    std::vector<int> eps;                    // epsilon-переходы
    std::vector<std::pair<int, int>> edges;  // (символ, цель)
    int accept = 0, priority = 0;            // accept: id токена
    bool skip = false;
};
struct NFA {
    std::vector<NFAState> states;
    int start = 0;
};

struct DFAState {
    std::array<int, kAlphabet> trans;  // -1 = перехода нет (пока нет ловушки)
    int accept = 0;                    // id токена
    bool skip = false;
    DFAState() { trans.fill(-1); }
};
struct DFA {
    std::vector<DFAState> states;
    std::vector<std::string> names;  // names[id] — имя токена, names[0] = "NONE"
    int start = 0, trap = -1;        // trap = -1, пока ДКА частичный
};

// Читает таблицу токенов: строки "ИМЯ  SKIP|TOKEN  регулярка", '#' — комментарий.
std::vector<TokenDef> loadTokenDefs(const std::string& path);
// Все регулярки -> один НКА (Томпсон). Бросает runtime_error на плохой регулярке.
NFA buildNFA(const std::vector<TokenDef>& defs);
// НКА -> частичный ДКА (subset construction).
DFA subsetConstruction(const NFA& nfa, const std::vector<TokenDef>& defs);
// Добавляет одно поглощающее непринимающее состояние и ведёт в него все отсутствующие переходы.
DFA completeWithTrap(const DFA& dfa);
// Хопкрофт: убирает недостижимые состояния и склеивает эквивалентные; ловушка сохраняется.
DFA minimize(const DFA& dfa);
// Полный конвейер: таблица токенов -> минимальный ДКА с ловушкой.
DFA buildMinimalDFA(const std::vector<TokenDef>& defs);

void exportJSON(const DFA& dfa, const std::string& path);
void exportCSV(const DFA& dfa, const std::string& path);
void exportHeader(const DFA& dfa, const std::string& path);

struct Token {
    std::string type, lexeme;  // type == "ERR" для символа, попавшего в ловушку
    size_t pos = 0;
};

class Lexer {
public:
    explicit Lexer(DFA dfa) : dfa_(std::move(dfa)) {}
    // Максимальное совпадение; WS/COMMENT пропускаются; непринятый символ -> Token "ERR".
    std::vector<Token> tokenize(const std::string& input) const;
    // Состояние после прогона всей строки (байты >= 128 сразу ведут в ловушку).
    int run(const std::string& input) const;
    // Принимает ли ДКА строку целиком одним токеном; имя токена кладёт в *type.
    bool acceptsWhole(const std::string& input, std::string* type = nullptr) const;
    const DFA& dfa() const { return dfa_; }

private:
    DFA dfa_;
};

// "IDENT(x) OP_ASSIGN(=) ERR(_)" — поток токенов в виде строки.
std::string describeTokens(const std::vector<Token>& toks);


struct TestCase {
    std::string name, input, expected;
    bool whole = false;
    bool bad() const { return expected == "REJECT" || expected.find("ERR(") != std::string::npos; }
};
std::vector<TestCase> loadTestCases(const std::string& path);
std::string runTestCase(const Lexer& lexer, const TestCase& tc);

}
