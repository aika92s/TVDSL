#include "funnylex.hpp"

#include <fstream>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>

namespace funnylex {

// таблица токенов
std::vector<TokenDef> loadTokenDefs(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open token file: " + path);
    std::vector<TokenDef> defs;
    std::set<std::string> seen;
    std::string line;
    while (std::getline(in, line)) {
        line = line.substr(0, line.find('#'));
        const char* ws = " \t\r\n";
        // line = <имя> <вид> <регулярка>
        size_t b1 = line.find_first_not_of(ws);
        if (b1 == std::string::npos) continue;  // пустая строка / комментарий
        size_t e1 = line.find_first_of(ws, b1);
        size_t b2 = line.find_first_not_of(ws, e1);
        if (e1 == std::string::npos || b2 == std::string::npos) continue;
        size_t e2 = line.find_first_of(ws, b2);

        TokenDef d;
        d.name = line.substr(b1, e1 - b1);
        std::string kind = line.substr(b2, e2 == std::string::npos ? e2 : e2 - b2);
        if (e2 != std::string::npos) d.regex = line.substr(e2);
        d.regex.erase(0, d.regex.find_first_not_of(" \t"));
        d.regex.erase(d.regex.find_last_not_of(" \t\r") + 1);
        if (d.regex.empty() || (kind != "SKIP" && kind != "TOKEN"))
            throw std::runtime_error("bad token line: " + line);
        if (!seen.insert(d.name).second) throw std::runtime_error("duplicate token: " + d.name);
        d.skip = (kind == "SKIP");
        d.priority = static_cast<int>(defs.size());
        defs.push_back(d);
    }
    return defs;
}

// регулярка to НКА (Томпсон)

namespace {

struct Frag { int s, e; }; //вход и выход куска автомата

class RegexBuilder {
public:
    explicit RegexBuilder(NFA& nfa) : nfa_(nfa) {}

    Frag build(const std::string& pattern) {
        pat_ = pattern;
        pos_ = 0;
        Frag f = alt();
        if (pos_ != pat_.size()) fail("unexpected ')' or trailing characters");
        return f;
    }

private:
    NFA& nfa_;
    std::string pat_; //регулярка
    size_t pos_ = 0;

    [[noreturn]] void fail(const std::string& msg) const {
        throw std::runtime_error(msg + " in regex '" + pat_ + "' at " + std::to_string(pos_));
    }

    //Возвращает true, если ещё есть непрочитанные символы
    bool more() const { return pos_ < pat_.size(); }

    //бавляет в nfa_.states пустое состояние
    int newState() {
        nfa_.states.emplace_back();
        return static_cast<int>(nfa_.states.size()) - 1;
    }

    //строит самый маленький кусок автомата из двух состояний s и e (ребра)
    Frag single(const std::vector<int>& symbols) {
        Frag f{newState(), newState()};
        for (int c : symbols) nfa_.states[f.s].edges.push_back({c, f.e});
        return f;
    }

    Frag wrap(Frag a, bool skipAllowed, bool repeatAllowed) {  // ?, +, * через epsilon
        Frag f{newState(), newState()};
        nfa_.states[f.s].eps.push_back(a.s);
        if (skipAllowed) nfa_.states[f.s].eps.push_back(f.e); // можно пройти мимо a (нужно для * и ?);
        nfa_.states[a.e].eps.push_back(f.e);
        if (repeatAllowed) nfa_.states[a.e].eps.push_back(a.s); // можно повторить a (нужно для * и +).
        return f;
    }

    // ветвление |
    Frag alt() {
        Frag left = concat();
        while (more() && pat_[pos_] == '|') {
            ++pos_;
            Frag right = concat();
            Frag f{newState(), newState()};
            nfa_.states[f.s].eps = {left.s, right.s};
            nfa_.states[left.e].eps.push_back(f.e);
            nfa_.states[right.e].eps.push_back(f.e);
            left = f;
        }
        return left;
    }

    Frag concat() {
        if (!more() || pat_[pos_] == '|' || pat_[pos_] == ')') {  // пустая ветка
            Frag f{newState(), newState()};
            nfa_.states[f.s].eps.push_back(f.e); //возвращается фрагмент из двух состояний с одним ε-переходом
            return f;
        }
        Frag f = repeat();
        while (more() && pat_[pos_] != '|' && pat_[pos_] != ')') {
            Frag g = repeat();
            nfa_.states[f.e].eps.push_back(g.s);
            f.e = g.e;
        }
        return f;
    }
    Frag repeat() {
        Frag a = atom();
        if (more()) {
            char c = pat_[pos_];
            if (c == '*') { ++pos_; return wrap(a, true, true); }
            if (c == '+') { ++pos_; return wrap(a, false, true); }
            if (c == '?') { ++pos_; return wrap(a, true, false); }
        }
        return a;
    }
    Frag atom() {
        char c = pat_[pos_++];
        if (c == '(') {
            Frag f = alt();
            if (!more() || pat_[pos_++] != ')') fail("missing ')'");
            return f;
        }
        if (c == '[') return single(charClass());
        if (c == '.') {
            std::vector<int> all;
            for (int i = 0; i < kAlphabet; ++i) all.push_back(i);
            return single(all);
        }
        if (c == '\\') return single({escaped()});
        if (std::string("|*+?)]").find(c) != std::string::npos) { --pos_; fail("unescaped special char"); }
        return single({static_cast<unsigned char>(c)});
    }
    int escaped() {  // символ после '\'
        if (!more()) fail("dangling backslash");
        char c = pat_[pos_++];
        return c == 'n' ? '\n' : c == 't' ? '\t' : c == 'r' ? '\r' : static_cast<unsigned char>(c);
    }
    int classChar() {
        if (!more()) fail("missing ']'");
        return pat_[pos_] == '\\' ? (++pos_, escaped()) : static_cast<unsigned char>(pat_[pos_++]);
    }
    std::vector<int> charClass() {  // после '[': символы, диапазоны a-z, отрицание ^
        bool negate = more() && pat_[pos_] == '^';
        if (negate) ++pos_;
        std::vector<bool> in(kAlphabet, false);
        bool any = false;
        while (more() && pat_[pos_] != ']') {
            int lo = classChar(), hi = lo;
            if (more() && pat_[pos_] == '-' && pos_ + 1 < pat_.size() && pat_[pos_ + 1] != ']') {
                ++pos_;
                hi = classChar();
                if (hi < lo) fail("bad range");
            }
            for (int v = lo; v <= hi; ++v) in[v] = true;
            any = true;
        }
        if (!more()) fail("missing ']'");
        ++pos_;
        if (!any && !negate) fail("empty class");
        std::vector<int> set;
        for (int i = 0; i < kAlphabet; ++i)
            if (in[i] != negate) set.push_back(i);
        return set;
    }
};

}

NFA buildNFA(const std::vector<TokenDef>& defs) {
    NFA nfa;
    nfa.states.emplace_back();  // создаёт общее стартовое состояние 0
    RegexBuilder rb(nfa);
    for (size_t i = 0; i < defs.size(); ++i) {
        Frag f;
        try {
            //для каждого токена строит фрагмент через RegexBuilder и соединяет 0 с его входом по ε.
            f = rb.build(defs[i].regex);
        } catch (const std::exception& ex) {
            throw std::runtime_error("token " + defs[i].name + ": " + ex.what());
        }
        nfa.states[0].eps.push_back(f.s);
        NFAState& acc = nfa.states[f.e];
        acc.accept = static_cast<int>(i) + 1;
        acc.priority = defs[i].priority;
        acc.skip = defs[i].skip;
    }
    return nfa;
}

// НКА to ДКА

namespace {
using StateSet = std::set<int>;
//добавляет все состояния, достижимые по ε-переходам
StateSet closure(const NFA& nfa, StateSet set) {
    std::vector<int> stack(set.begin(), set.end());
    while (!stack.empty()) {
        int s = stack.back();
        stack.pop_back();
        for (int t : nfa.states[s].eps)
            if (set.insert(t).second) stack.push_back(t);
    }
    return set;
}
}
//Превращает НКА в ДКА (не законченный)
DFA subsetConstruction(const NFA& nfa, const std::vector<TokenDef>& defs) {
    DFA dfa;
    dfa.names.push_back("NONE");
    for (const auto& d : defs) dfa.names.push_back(d.name);

    std::map<StateSet, int> id;
    std::vector<StateSet> sets;

    //выдаёт номер ДКА-состоянию
    auto intern = [&](const StateSet& s) {
        auto ins = id.emplace(s, static_cast<int>(sets.size()));
        if (ins.second) { sets.push_back(s); dfa.states.emplace_back(); }
        return ins.first->second;
    };
    intern(closure(nfa, {nfa.start}));

    for (size_t cur = 0; cur < sets.size(); ++cur) {  // sets растёт в процессе (очередь)
        const StateSet set = sets[cur];
        int bestPriority = -1;  // принимает токен с минимальным priority
        std::map<int, StateSet> moves;
        for (int s : set) {
            const NFAState& st = nfa.states[s];
            if (st.accept && (bestPriority < 0 || st.priority < bestPriority)) {
                bestPriority = st.priority;
                dfa.states[cur].accept = st.accept;
                dfa.states[cur].skip = st.skip;
            }
            for (auto [c, t] : st.edges) moves[c].insert(t);
        }
        for (auto& [c, target] : moves) {
            int t = intern(closure(nfa, target));
            dfa.states[cur].trans[c] = t;
        }
    }
    return dfa;
}
    //Добавляет одно новое состояние (ловушку) и заменяет все -1 на него
DFA completeWithTrap(const DFA& dfa) {
    if (dfa.trap != -1) return dfa;
    DFA out = dfa;
    out.trap = static_cast<int>(out.states.size());
    out.states.emplace_back();
    for (auto& st : out.states)
        for (int& t : st.trans)
            if (t == -1) t = out.trap;
    return out;
}
    //cостояния, до которых не дойти, выбрасываются (обход в ширину)
namespace {
DFA removeUnreachable(const DFA& d) {
    std::vector<int> newId(d.states.size(), -1), order{d.start};
    newId[d.start] = 0;
    for (size_t i = 0; i < order.size(); ++i)
        for (int t : d.states[order[i]].trans)
            if (t != -1 && newId[t] == -1) {
                newId[t] = static_cast<int>(order.size());
                order.push_back(t);
            }

    //переходы переписываются на новые номера
    DFA out;
    out.names = d.names;
    for (int old : order) {
        DFAState st = d.states[old];
        for (int& t : st.trans) t = (t == -1) ? -1 : newId[t];
        out.states.push_back(st);
    }
    out.trap = d.trap == -1 ? -1 : newId[d.trap];
    return out;
}
}

DFA minimize(const DFA& input) {
    DFA dfa = removeUnreachable(completeWithTrap(input));
    const int n = static_cast<int>(dfa.states.size());

    // inv[c][t] — состояния, переходящие в t по символу c
    std::vector<std::vector<std::vector<int>>> inv(kAlphabet, std::vector<std::vector<int>>(n));
    for (int s = 0; s < n; ++s)
        for (int c = 0; c < kAlphabet; ++c) inv[c][dfa.states[s].trans[c]].push_back(s);

    // начальное разбиение: по токену, который принимает состояние
    std::vector<std::set<int>> P;
    std::vector<int> blockOf(n);
    std::map<int, int> byToken;
    for (int s = 0; s < n; ++s) {
        auto it = byToken.emplace(dfa.states[s].accept, static_cast<int>(P.size()));
        if (it.second) P.emplace_back();
        P[it.first->second].insert(s);
        blockOf[s] = it.first->second;
    }

    std::set<std::pair<int, int>> W;  // очередь расщепителей (блок, символ)
    for (int b = 0; b < static_cast<int>(P.size()); ++b)
        for (int c = 0; c < kAlphabet; ++c) W.insert({b, c});

    while (!W.empty()) {
        auto [a, c] = *W.begin();
        W.erase(W.begin());
        std::map<int, std::vector<int>> hit;  // блок -> его состояния, ведущие в блок a по c
        std::set<int> X;
        for (int t : P[a])
            for (int s : inv[c][t]) X.insert(s);
        for (int s : X) hit[blockOf[s]].push_back(s);

        for (auto& [y, part] : hit) {
            if (part.size() == P[y].size()) continue;  // блок не делится
            int nb = static_cast<int>(P.size());
            P.emplace_back();
            for (int s : part) { P[y].erase(s); P[nb].insert(s); blockOf[s] = nb; }
            for (int sym = 0; sym < kAlphabet; ++sym) {
                if (W.count({y, sym})) W.insert({nb, sym});
                else W.insert({P[y].size() <= P[nb].size() ? y : nb, sym});  // меньшая половина
            }
        }
    }

    // фактор-автомат; нумерация блоков в порядке обхода от старта
    std::vector<int> num(P.size(), -1), order;
    auto visit = [&](int b) { if (num[b] == -1) { num[b] = static_cast<int>(order.size()); order.push_back(b); } };
    visit(blockOf[dfa.start]);
    for (size_t i = 0; i < order.size(); ++i)
        for (int t : dfa.states[*P[order[i]].begin()].trans) visit(blockOf[t]);

    DFA out;
    out.names = dfa.names;
    for (int b : order) {
        DFAState st = dfa.states[*P[b].begin()];
        for (int& t : st.trans) t = num[blockOf[t]];
        out.states.push_back(st);
    }
    out.start = num[blockOf[dfa.start]];
    out.trap = num[blockOf[dfa.trap]];
    return out;
}

DFA buildMinimalDFA(const std::vector<TokenDef>& defs) {
    return minimize(subsetConstruction(buildNFA(defs), defs));
}

// экспорт таблицы

namespace {
std::ofstream openOut(const std::string& path) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("cannot write: " + path);
    return out;
}
}

void exportJSON(const DFA& dfa, const std::string& path) {
    auto out = openOut(path);
    out << "{\n  \"alphabet_size\": " << kAlphabet << ",\n  \"num_states\": " << dfa.states.size()
        << ",\n  \"start_state\": " << dfa.start << ",\n  \"trap_state\": " << dfa.trap
        << ",\n  \"note\": \"transitions[c], c = ASCII code; bytes >= 128 must go to trap_state; "
           "accept = null: not accepting; skip = true: drop the lexeme (WS/COMMENT)\",\n  \"states\": [\n";
    for (size_t i = 0; i < dfa.states.size(); ++i) {
        const DFAState& st = dfa.states[i];
        out << "    {\"id\": " << i << ", \"accept\": ";
        if (st.accept) out << '"' << dfa.names[st.accept] << '"'; else out << "null";
        out << ", \"skip\": " << (st.skip ? "true" : "false") << ", \"transitions\": [";
        for (int c = 0; c < kAlphabet; ++c) out << st.trans[c] << (c + 1 < kAlphabet ? "," : "");
        out << "]}" << (i + 1 < dfa.states.size() ? "," : "") << "\n";
    }
    out << "  ]\n}\n";
}

void exportCSV(const DFA& dfa, const std::string& path) {
    auto out = openOut(path);
    out << "state,accept,skip,is_start,is_trap";
    for (int c = 0; c < kAlphabet; ++c) out << ",c" << c;
    out << "\n";
    for (size_t i = 0; i < dfa.states.size(); ++i) {
        const DFAState& st = dfa.states[i];
        out << i << "," << dfa.names[st.accept] << "," << st.skip << "," << (int(i) == dfa.start) << ","
            << (int(i) == dfa.trap);
        for (int t : st.trans) out << "," << t;
        out << "\n";
    }
}

void exportHeader(const DFA& dfa, const std::string& path) {
    auto out = openOut(path);
    out << "// Generated by funny_lexgen. Do not edit.\n#pragma once\n\nnamespace funny_dfa {\n\n"
        << "constexpr int kAlphabetSize = " << kAlphabet << ";\n"
        << "constexpr int kNumStates = " << dfa.states.size() << ";\n"
        << "constexpr int kStartState = " << dfa.start << ";\n"
        << "// Ловушка: непринимающая, поглощающая. Байты >= 128 потребитель отправляет в неё сам.\n"
        << "constexpr int kTrapState = " << dfa.trap << ";\n\nenum TokenId {\n";
    for (size_t i = 0; i < dfa.names.size(); ++i) out << "    TOK_" << dfa.names[i] << " = " << i << ",\n";
    out << "};\n\nconstexpr const char* kTokenNames[] = {";
    for (size_t i = 0; i < dfa.names.size(); ++i) out << (i ? ", " : "") << '"' << dfa.names[i] << '"';
    out << "};\n\nconstexpr int kTransitions[kNumStates][kAlphabetSize] = {\n";
    for (const auto& st : dfa.states) {
        out << "    {";
        for (int c = 0; c < kAlphabet; ++c) out << st.trans[c] << (c + 1 < kAlphabet ? "," : "");
        out << "},\n";
    }
    out << "};\n\nconstexpr int kAcceptToken[kNumStates] = {";
    for (size_t i = 0; i < dfa.states.size(); ++i) out << (i ? "," : "") << dfa.states[i].accept;
    out << "};\n\nconstexpr bool kIsSkip[kNumStates] = {";
    for (size_t i = 0; i < dfa.states.size(); ++i) out << (i ? "," : "") << (dfa.states[i].skip ? "true" : "false");
    out << "};\n\n}  // namespace funny_dfa\n";
}

// лексер

int Lexer::run(const std::string& input) const {
    int s = dfa_.start;
    for (char ch : input) {
        unsigned char c = ch;
        s = c < kAlphabet ? dfa_.states[s].trans[c] : dfa_.trap;
        if (s == dfa_.trap) break;
    }
    return s;
}

bool Lexer::acceptsWhole(const std::string& input, std::string* type) const {
    int s = run(input);
    if (s == dfa_.trap || !dfa_.states[s].accept) return false;
    if (type) *type = dfa_.names[dfa_.states[s].accept];
    return true;
}

    //режет текст на токены, каждый раз беря самую длинную подходящую лексему.
std::vector<Token> Lexer::tokenize(const std::string& input) const {
    std::vector<Token> out;
    size_t i = 0;
    while (i < input.size()) {
        int s = dfa_.start;
        size_t end = i;  // конец последнего принимающего префикса (end == i: ничего не принято)
        int tok = 0;
        bool skip = false;
        for (size_t j = i; j < input.size(); ++j) {
            unsigned char c = input[j];
            s = c < kAlphabet ? dfa_.states[s].trans[c] : dfa_.trap;
            if (s == dfa_.trap) break;
            if (dfa_.states[s].accept) {
                end = j + 1;
                tok = dfa_.states[s].accept;
                skip = dfa_.states[s].skip;
            }
        }
        if (end == i) {  // ловушка без единого принятого символа: один символ -> ошибка
            out.push_back({"ERR", input.substr(i, 1), i});
            ++i;
            continue;
        }
        if (!skip) out.push_back({dfa_.names[tok], input.substr(i, end - i), i});
        i = end;
    }
    return out;
}

std::string describeTokens(const std::vector<Token>& toks) {
    std::string s;
    for (const auto& t : toks) s += (s.empty() ? "" : " ") + t.type + "(" + t.lexeme + ")";
    return s;
}

// тесты из data/tests.jsonl

namespace {
std::vector<std::string> jsonStrings(const std::string& line) {
    std::vector<std::string> out;
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] != '"') continue;
        std::string s;
        for (++i; i < line.size() && line[i] != '"'; ++i) {
            if (line[i] != '\\' || i + 1 >= line.size()) { s += line[i]; continue; }
            char n = line[++i];
            if (n == 'n') s += '\n';
            else if (n == 't') s += '\t';
            else if (n == 'r') s += '\r';
            else if (n == 'u' && i + 4 < line.size()) {
                s += static_cast<char>(std::stoul(line.substr(i + 1, 4), nullptr, 16) & 0xFF);
                i += 4;
            } else s += n;
        }
        out.push_back(s);
    }
    return out;
}
}

std::vector<TestCase> loadTestCases(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open tests file: " + path);
    std::vector<TestCase> tests;
    std::string line;
    while (std::getline(in, line)) {
        auto strs = jsonStrings(line);
        if (strs.empty()) continue;
        std::map<std::string, std::string> kv;
        for (size_t i = 0; i + 1 < strs.size(); i += 2) kv[strs[i]] = strs[i + 1];
        tests.push_back({kv["name"], kv["input"], kv["expected"], kv["mode"] == "whole"});
    }
    return tests;
}

std::string runTestCase(const Lexer& lexer, const TestCase& tc) {
    if (!tc.whole) return describeTokens(lexer.tokenize(tc.input));
    std::string type;
    return lexer.acceptsWhole(tc.input, &type) ? type : "REJECT";
}

}