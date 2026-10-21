#include "parser.hpp"

#include <algorithm>
#include <map>

namespace funnyparse {
namespace {

// Токен для парсера: токен лексера + строка/столбец. Конец ввода — токен типа "EOF".
struct Tok {
    std::string type, lexeme;
    size_t pos = 0;
    int line = 0, col = 0;
};

// Синтаксическая ошибка; ловится в режиме паники и превращается в Diagnostic.
struct ParseError {
    int line, col;
    std::string msg;
};

const char* const kArrowUtf8 = "\xE2\x86\x92";  // "→" — в грамматике Funny это импликация
const int kMaxDepth = 200;                      // защита стека от вложенности вида ((((((...

// Смещения начал строк. Перевод строки — \n, \r\n или одиночный \r
std::vector<size_t> lineStarts(const std::string& s) {
    std::vector<size_t> ls{0};
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\n') ls.push_back(i + 1);
        else if (s[i] == '\r' && (i + 1 >= s.size() || s[i + 1] != '\n')) ls.push_back(i + 1);
    }
    return ls;
}

// Смещение в тексте -> строка и столбец (с 1) бинарным поиском по началам строк.
void lineCol(const std::vector<size_t>& ls, size_t pos, int& line, int& col) {
    size_t i = static_cast<size_t>(std::upper_bound(ls.begin(), ls.end(), pos) - ls.begin()) - 1;
    line = static_cast<int>(i) + 1;
    col = static_cast<int>(pos - ls[i]) + 1;
}

// Как называть ожидаемый токен в сообщении: "';'", "'returns'", "identifier".
std::string expectedName(const std::string& type) {
    static const std::map<std::string, std::string> ops = {
        {"OP_LPAREN", "'('"}, {"OP_RPAREN", "')'"}, {"OP_LBRACKET", "'['"}, {"OP_RBRACKET", "']'"},
        {"OP_LBRACE", "'{'"}, {"OP_RBRACE", "'}'"}, {"OP_COMMA", "','"},    {"OP_SEMI", "';'"},
        {"OP_COLON", "':'"},  {"OP_PIPE", "'|'"},   {"OP_ASSIGN", "'='"},   {"OP_ARROW", "'=>'"}};
    auto it = ops.find(type);
    if (it != ops.end()) return it->second;
    if (type == "IDENT") return "identifier";
    if (type == "INT") return "number";
    if (type.compare(0, 3, "KW_") == 0) {
        std::string w = type.substr(3);
        for (auto& c : w) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return "'" + w + "'";
    }
    return type;
}

// Как назвать найденный токен в сообщении.
std::string found(const Tok& t) { return t.type == "EOF" ? "end of input" : "'" + t.lexeme + "'"; }

// Подсказка, если вместо идентификатора стоит ключевое слово: "(`length` is a reserved word)".
std::string reservedHint(const Tok& t) {
    return t.type.compare(0, 3, "KW_") == 0 ? " ('" + t.lexeme + "' is a reserved word)" : "";
}

bool isCompareOp(const std::string& type) {
    return type == "OP_EQ" || type == "OP_NEQ" || type == "OP_LE" || type == "OP_GE" || type == "OP_LT" ||
           type == "OP_GT";
}

bool isArithOp(const std::string& type) {
    return type == "OP_PLUS" || type == "OP_MINUS" || type == "OP_STAR" || type == "OP_SLASH";
}

// Может ли токен начинать арифметическое выражение.
bool startsExpr(const std::string& type) {
    return type == "INT" || type == "IDENT" || type == "KW_LENGTH" || type == "OP_MINUS" || type == "OP_LPAREN";
}

// Парсер рекурсивного спуска: по функции на нетерминал, выбор ветки по одному текущему токену.
class Parser {
public:
    Parser(std::vector<Tok> toks, std::vector<Diagnostic>& errs) : t_(std::move(toks)), errs_(errs) {}

    // program = { function | formula } ; пустая программа — ошибка.
    NodePtr program() {
        auto root = std::make_unique<Node>(Kind::Program, "", 1, 1);
        if (atEOF()) {
            errs_.push_back({cur().line, cur().col, "empty program: expected at least one function or formula"});
            return root;
        }
        while (!atEOF()) {
            size_t start = p_;
            try {
                root->kids.push_back(definition());
            } catch (const ParseError& e) {
                report(e);
                syncTop(start);
                if (p_ == start) ++p_;  // гарантия прогресса
            }
        }
        return root;
    }

private:
    std::vector<Tok> t_;
    size_t p_ = 0;
    std::vector<Diagnostic>& errs_;
    int depth_ = 0;

    //  работа с токенами

    // Текущий токен (последний всегда EOF).
    const Tok& cur() const { return t_[p_]; }
    // Токен через k позиций вперёд (не дальше EOF).
    const Tok& peek(size_t k = 1) const { return t_[std::min(p_ + k, t_.size() - 1)]; }
    bool is(const char* type) const { return cur().type == type; }
    bool atEOF() const { return cur().type == "EOF"; }
    // Забирает текущий токен; на EOF остаётся на месте.
    Tok advance() {
        Tok t = cur();
        if (!atEOF()) ++p_;
        return t;
    }
    // Забирает токен, если он нужного типа.
    bool accept(const char* type) {
        if (!is(type)) return false;
        advance();
        return true;
    }
    // Бросает ошибку в позиции текущего токена.
    [[noreturn]] void fail(const std::string& msg) const { throw ParseError{cur().line, cur().col, msg}; }
    // Бросает ошибку в заданной позиции.
    [[noreturn]] void failAt(const Node& n, const std::string& msg) const { throw ParseError{n.line, n.col, msg}; }
    // Требует токен типа type: "expected ';' after assignment, found '}'".
    Tok expect(const char* type, const std::string& ctx = "") {
        if (!is(type))
            fail("expected " + expectedName(type) + (ctx.empty() ? "" : " " + ctx) + ", found " + found(cur()) +
                 (std::string(type) == "IDENT" ? reservedHint(cur()) : ""));
        return advance();
    }
    // Требует закрывающую скобку и напоминает, где была открывающая.
    Tok expectClose(const char* type, const Tok& open) {
        if (!is(type))
            fail("expected " + expectedName(type) + " to close '" + open.lexeme + "' opened at " +
                 std::to_string(open.line) + ":" + std::to_string(open.col) + ", found " + found(cur()));
        return advance();
    }
    // Новый узел с позицией токена.
    NodePtr mk(Kind k, const Tok& t, std::string text = "") const {
        return std::make_unique<Node>(k, std::move(text), t.line, t.col);
    }
    // Записывает ошибку в список диагностик.
    void report(const ParseError& e) { errs_.push_back({e.line, e.col, e.msg}); }

    // Счётчик глубины рекурсии: бросает ошибку при слишком глубокой вложенности.
    struct Guard {
        Parser& p;
        explicit Guard(Parser& pp) : p(pp) {
            if (++p.depth_ > kMaxDepth) {
                --p.depth_;
                p.fail("nesting is too deep");
            }
        }
        ~Guard() { --p.depth_; }
    };

    //  восстановление

    // Может ли токен стоять последним в определении (признак того, что дальше начнётся новое).
    static bool endsDefinition(const Tok& t) {
        return t.type == "OP_RBRACE" || t.type == "OP_SEMI" || t.type == "OP_RPAREN" || t.type == "OP_RBRACKET" ||
               t.type == "INT" || t.type == "IDENT" || t.type == "KW_TRUE" || t.type == "KW_FALSE";
    }
    // Похоже ли текущее место на начало определения: IDENT "(" после конца предыдущего определения.
    bool defStart() const {
        return p_ > 0 && is("IDENT") && peek().type == "OP_LPAREN" && endsDefinition(t_[p_ - 1]);
    }
    // После ошибки на верхнем уровне: пропускаем токены до начала следующего определения (вне скобок {}).
    void syncTop(size_t start) {
        int depth = 0;
        for (; !atEOF(); ++p_) {
            if (is("OP_LBRACE")) ++depth;
            else if (is("OP_RBRACE") && depth > 0) --depth;
            else if (depth == 0 && p_ > start && defStart()) return;
        }
    }
    // После ошибки в операторе: пропускаем до ';' (съедаем его), до '}' или до слова-начала оператора.
    void syncStmt() {
        while (!atEOF()) {
            if (is("OP_SEMI")) {
                advance();
                return;
            }
            if (is("OP_RBRACE") || is("OP_LBRACE") || is("KW_IF") || is("KW_WHILE") || is("KW_ASSERT") ||
                is("KW_ASSUME"))
                return;
            advance();
        }
    }

    //  определения

    // definition = function | formula; ветка выбирается по токену после ')': "=>" — формула.
    NodePtr definition() {
        if (!is("IDENT")) fail("expected function or formula name, found " + found(cur()) + reservedHint(cur()));
        Tok name = advance();
        expect("OP_LPAREN", "after the name");
        auto params = varDefList(Kind::Params, true);
        expect("OP_RPAREN", "after parameters");
        if (is("OP_ARROW")) return formulaRest(name, std::move(params));
        return functionRest(name, std::move(params));
    }

    // formula = name "(" params ")" "=>" predicate ; (';' в описании Funny — конец правила EBNF, не токен).
    NodePtr formulaRest(const Tok& name, NodePtr params) {
        advance();  // =>
        auto f = mk(Kind::Formula, name, name.lexeme);
        f->kids.push_back(std::move(params));
        f->kids.push_back(predicate());
        return f;
    }

    // Остаток функции после ")": [requires P] returns ... [ensures P] [uses ...] statement.
    NodePtr functionRest(const Tok& name, NodePtr params) {
        auto f = mk(Kind::Function, name, name.lexeme);
        f->kids.push_back(std::move(params));
        if (is("KW_REQUIRES")) {
            auto r = mk(Kind::Requires, advance());
            r->kids.push_back(predicate());
            f->kids.push_back(std::move(r));
        }
        expect("KW_RETURNS", "in function header");
        f->kids.push_back(varDefList(Kind::Returns, false));
        if (is("KW_ENSURES")) {
            auto e = mk(Kind::Ensures, advance());
            e->kids.push_back(predicate());
            f->kids.push_back(std::move(e));
        }
        if (is("KW_USES")) {
            advance();
            f->kids.push_back(varDefList(Kind::Locals, false));
        }
        f->kids.push_back(statement());
        return f;
    }

    // Список "x: int, y: int[]"; allowEmpty — пустой список разрешён (параметры).
    NodePtr varDefList(Kind k, bool allowEmpty) {
        auto list = mk(k, cur());
        if (allowEmpty && is("OP_RPAREN")) return list;
        do {
            list->kids.push_back(varDef());
        } while (accept("OP_COMMA"));
        return list;
    }

    // variableDef = identifier ":" ("int" | "int[]").
    NodePtr varDef() {
        Tok id = expect("IDENT", "as variable name");
        expect("OP_COLON", "after variable name");
        if (!is("KW_INT")) fail("expected type 'int' or 'int[]', found " + found(cur()));
        advance();
        std::string type = "int";
        if (accept("OP_LBRACKET")) {
            expect("OP_RBRACKET", "in type 'int[]'");
            type = "int[]";
        }
        auto v = mk(Kind::VarDef, id, id.lexeme);
        v->extra = type;
        return v;
    }

    // операторы

    // statement = assignment | conditional | loop | block | assert | assume.
    NodePtr statement() {
        Guard g(*this);
        if (is("OP_LBRACE")) return block();
        if (is("KW_IF")) return ifStmt();
        if (is("KW_WHILE")) return whileStmt();
        if (is("KW_ASSERT") || is("KW_ASSUME")) return assertStmt();
        if (is("IDENT")) return assignment();
        if (is("KW_ELSE")) fail("'else' without matching 'if'");
        if (isCompareOp(cur().type)) fail("unexpected " + found(cur()) + " (comparisons cannot be chained)");
        fail("expected statement, found " + found(cur()));
    }

    // block = "{" { statement } "}" ; ошибка внутри блока не обрывает блок: синхронизируемся и идём дальше.
    NodePtr block() {
        Tok open = advance();
        auto b = mk(Kind::Block, open);
        while (!is("OP_RBRACE")) {
            if (atEOF()) expectClose("OP_RBRACE", open);
            size_t start = p_;
            try {
                b->kids.push_back(statement());
            } catch (const ParseError& e) {
                report(e);
                syncStmt();
                if (p_ == start) advance();  // гарантия прогресса
            }
        }
        advance();  // }
        return b;
    }

    // assignment: x = e; | a[i] = e; | x, y = f(...); ; для одной переменной вызов — обычное присваивание.
    NodePtr assignment() {
        Tok id = advance();
        if (is("OP_LBRACKET")) {
            advance();
            auto n = mk(Kind::ArrayAssign, id, id.lexeme);
            n->kids.push_back(expr());
            expect("OP_RBRACKET", "after array index");
            expect("OP_ASSIGN", "in array element assignment");
            n->kids.push_back(expr());
            expect("OP_SEMI", "after assignment");
            return n;
        }
        std::vector<Tok> names{id};
        while (accept("OP_COMMA")) names.push_back(expect("IDENT", "as variable name"));
        expect("OP_ASSIGN", "in assignment");
        NodePtr rhs = expr();
        if (names.size() > 1 && rhs->kind != Kind::Call)
            failAt(*rhs, "tuple assignment requires a function call on the right side");
        expect("OP_SEMI", "after assignment");
        if (names.size() == 1) {
            auto n = mk(Kind::Assign, id, id.lexeme);
            n->kids.push_back(std::move(rhs));
            return n;
        }
        auto n = mk(Kind::TupleAssign, id);
        auto targets = mk(Kind::Targets, id);
        for (const auto& t : names) targets->kids.push_back(mk(Kind::Var, t, t.lexeme));
        n->kids.push_back(std::move(targets));
        n->kids.push_back(std::move(rhs));
        return n;
    }

    // conditional = "if" "(" condition ")" statement ["else" statement]; else цепляется к ближайшему if.
    NodePtr ifStmt() {
        auto n = mk(Kind::If, advance());
        expect("OP_LPAREN", "after 'if'");
        n->kids.push_back(condition());
        expect("OP_RPAREN", "after condition");
        n->kids.push_back(statement());
        if (accept("KW_ELSE")) n->kids.push_back(statement());
        return n;
    }

    // loop = "while" "(" condition ")" ["invariant" predicate] statement.
    NodePtr whileStmt() {
        auto n = mk(Kind::While, advance());
        expect("OP_LPAREN", "after 'while'");
        n->kids.push_back(condition());
        expect("OP_RPAREN", "after condition");
        if (is("KW_INVARIANT")) {
            auto inv = mk(Kind::Invariant, advance());
            inv->kids.push_back(predicate());
            n->kids.push_back(std::move(inv));
        }
        n->kids.push_back(statement());
        return n;
    }

    // assert predicate ; | assume predicate ;
    NodePtr assertStmt() {
        Tok kw = advance();
        auto n = mk(kw.type == "KW_ASSERT" ? Kind::Assert : Kind::Assume, kw);
        n->kids.push_back(predicate());
        expect("OP_SEMI", "after " + kw.lexeme);
        return n;
    }

    // арифметика

    // expr = term { ("+" | "-") term } — левоассоциативно.
    NodePtr expr() {
        NodePtr l = term();
        while (is("OP_PLUS") || is("OP_MINUS")) {
            Tok op = advance();
            auto n = mk(Kind::Binary, op, op.lexeme);
            n->kids.push_back(std::move(l));
            n->kids.push_back(term());
            l = std::move(n);
        }
        return l;
    }

    // term = unary { ("*" | "/") unary } — левоассоциативно.
    NodePtr term() {
        NodePtr l = unary();
        while (is("OP_STAR") || is("OP_SLASH")) {
            Tok op = advance();
            auto n = mk(Kind::Binary, op, op.lexeme);
            n->kids.push_back(std::move(l));
            n->kids.push_back(unary());
            l = std::move(n);
        }
        return l;
    }

    // unary = "-" unary | primary — унарный минус самый приоритетный.
    NodePtr unary() {
        Guard g(*this);
        if (is("OP_MINUS")) {
            auto n = mk(Kind::Neg, advance());
            n->kids.push_back(unary());
            return n;
        }
        return primary();
    }

    // primary = number | name | name "(" args ")" | name "[" expr "]" | length "(" args ")" | "(" expr ")".
    NodePtr primary() {
        if (is("INT")) {
            Tok t = advance();
            return mk(Kind::Number, t, t.lexeme);
        }
        if (is("IDENT") || is("KW_LENGTH")) {
            Tok id = advance();
            if (is("OP_LPAREN") || id.type == "KW_LENGTH") {
                auto call = mk(Kind::Call, id, id.lexeme);
                Tok open = expect("OP_LPAREN", "after 'length'");
                if (!is("OP_RPAREN")) {
                    do {
                        call->kids.push_back(expr());
                    } while (accept("OP_COMMA"));
                }
                expectClose("OP_RPAREN", open);
                return call;
            }
            if (is("OP_LBRACKET")) {
                Tok open = advance();
                auto idx = mk(Kind::Index, id, id.lexeme);
                idx->kids.push_back(expr());
                expectClose("OP_RBRACKET", open);
                return idx;
            }
            return mk(Kind::Var, id, id.lexeme);
        }
        if (is("OP_LPAREN")) {
            Tok open = advance();
            NodePtr e = expr();
            expectClose("OP_RPAREN", open);
            return e;
        }
        fail("expected expression, found " + found(cur()));
    }

    //  условия и предикаты

    // condition — булево без кванторов и ссылок на формулы (для if/while).
    NodePtr condition() { return implies(false); }
    // predicate — условие плюс кванторы и ссылки на формулы (requires/ensures/invariant/formula/assert).
    // Импликации в предикатах нет (в грамматике Funny она есть только у condition).
    NodePtr predicate() {
        NodePtr p = implies(true);
        if (is("OP_IMPLIES")) fail("implication is allowed only in conditions, not in predicates, found " + found(cur()));
        return p;
    }

    // implies = or [ "->" implies ] — только в condition (pred == false); правоассоциативно, самый слабый приоритет.
    // В режиме predicate — просто or.
    NodePtr implies(bool pred) {
        NodePtr l = orExpr(pred);
        if (!pred && is("OP_IMPLIES")) {
            auto n = mk(Kind::Implies, advance());
            n->kids.push_back(std::move(l));
            n->kids.push_back(implies(pred));
            return n;
        }
        return l;
    }

    // or = and { "or" and } — левоассоциативно.
    NodePtr orExpr(bool pred) {
        NodePtr l = andExpr(pred);
        while (is("KW_OR")) {
            auto n = mk(Kind::Or, advance());
            n->kids.push_back(std::move(l));
            n->kids.push_back(andExpr(pred));
            l = std::move(n);
        }
        return l;
    }

    // and = not { "and" not } — левоассоциативно.
    NodePtr andExpr(bool pred) {
        NodePtr l = notExpr(pred);
        while (is("KW_AND")) {
            auto n = mk(Kind::And, advance());
            n->kids.push_back(std::move(l));
            n->kids.push_back(notExpr(pred));
            l = std::move(n);
        }
        return l;
    }

    // not = "not" not | boolPrimary — отрицание самое приоритетное.
    NodePtr notExpr(bool pred) {
        Guard g(*this);
        if (is("KW_NOT")) {
            auto n = mk(Kind::Not, advance());
            n->kids.push_back(notExpr(pred));
            return n;
        }
        return boolPrimary(pred);
    }

    // Скобка "(" начинает арифметику, если после парной ")" стоит арифметический оператор или сравнение:
    // "(a+b) > c" — выражение, "(a > b) and c" — булево в скобках. Единственный просмотр вперёд за пределы 1 токена.
    bool parenIsExpr() const {
        int depth = 0;
        for (size_t i = p_; i < t_.size(); ++i) {
            if (t_[i].type == "OP_LPAREN") ++depth;
            else if (t_[i].type == "OP_RPAREN" && --depth == 0) {
                const std::string& nx = t_[std::min(i + 1, t_.size() - 1)].type;
                return isArithOp(nx) || isCompareOp(nx);
            }
        }
        return false;
    }

    // boolPrimary = true | false | quantifier | "(" implies ")" | comparison | formulaRef.
    NodePtr boolPrimary(bool pred) {
        if (is("KW_TRUE") || is("KW_FALSE")) {
            Tok t = advance();
            return mk(Kind::BoolConst, t, t.lexeme);
        }
        if (is("KW_FORALL") || is("KW_EXISTS")) {
            if (!pred) fail("quantifiers are allowed only in predicates, not in conditions");
            Tok q = advance();
            Tok open = expect("OP_LPAREN", "after '" + q.lexeme + "'");
            NodePtr v = varDef();
            auto n = mk(q.type == "KW_FORALL" ? Kind::Forall : Kind::Exists, q, v->text);
            n->extra = v->extra;
            expect("OP_PIPE", "after quantifier variable");
            n->kids.push_back(predicate());
            expectClose("OP_RPAREN", open);
            return n;
        }
        if (is("OP_LPAREN") && !parenIsExpr()) {
            Tok open = advance();
            NodePtr inner = implies(pred);
            expectClose("OP_RPAREN", open);
            return inner;
        }
        if (!startsExpr(cur().type)) fail(std::string("expected ") + (pred ? "predicate" : "condition") + ", found " + found(cur()));
        return comparison(pred);
    }

    // comparison = expr cmpOp expr; в предикате одиночный вызов "f(...)" без сравнения — ссылка на формулу.
    NodePtr comparison(bool pred) {
        Tok first = cur();
        NodePtr l = expr();
        if (isCompareOp(cur().type)) {
            Tok op = advance();
            auto n = mk(Kind::Compare, op, op.lexeme);
            n->kids.push_back(std::move(l));
            n->kids.push_back(expr());
            return n;
        }
        if (pred && first.type == "IDENT" && l->kind == Kind::Call) {
            auto ref = std::move(l);
            ref->kind = Kind::FormulaRef;
            return ref;
        }
        std::string hint = is("OP_ASSIGN") ? " (did you mean '=='?)" : "";
        fail("expected comparison operator (==, !=, <, <=, >, >=), found " + found(cur()) + hint);
    }
};

std::vector<funnylex::Token> dropCommentTails(const std::string& src, const std::vector<funnylex::Token>& raw) {
    std::vector<funnylex::Token> out;
    size_t prevEnd = 0;    // конец предыдущего оставленного токена
    size_t skipUntil = 0;  // всё до этого смещения — хвост комментария
    for (const auto& t : raw) {
        if (t.pos < skipUntil) continue;
        if (t.type == "ERR") {
            std::string gap = src.substr(prevEnd, t.pos - prevEnd);  // только пропущенное лексером: пробелы и комментарии
            size_t nl = gap.find_last_of("\r\n");
            std::string last = nl == std::string::npos ? gap : gap.substr(nl + 1);
            size_t k = last.find_first_not_of(" \t");
            if (k != std::string::npos && last.compare(k, 2, "//") == 0) {
                size_t e = src.find_first_of("\r\n", t.pos);
                skipUntil = e == std::string::npos ? src.size() : e;
                prevEnd = skipUntil;
                continue;
            }
        }
        out.push_back(t);
        prevEnd = t.pos + t.lexeme.size();
    }
    return out;
}

}

// Лексер -> токены с позициями (ERR склеиваются в диагностики, "→" превращается в OP_IMPLIES) -> Parser.
ParseResult parseProgram(const std::string& source, const funnylex::Lexer& lexer) {
    ParseResult res;
    auto ls = lineStarts(source);
    std::vector<Tok> toks;

    auto addTok = [&](const std::string& type, const std::string& lexeme, size_t pos) {
        Tok t{type, lexeme, pos, 0, 0};
        lineCol(ls, pos, t.line, t.col);
        toks.push_back(t);
    };
    auto lexError = [&](const std::string& bad, size_t pos) {
        Diagnostic d;
        lineCol(ls, pos, d.line, d.col);
        d.message = std::string("unexpected character") + (bad.size() > 1 ? "s" : "") + " '" + bad + "'";
        if (bad == "_") d.message += " (identifiers may contain only letters and digits)";
        res.errors.push_back(d);
    };

    auto raw = dropCommentTails(source, lexer.tokenize(source));
    for (size_t i = 0; i < raw.size();) {
        if (raw[i].type != "ERR") {
            addTok(raw[i].type, raw[i].lexeme, raw[i].pos);
            ++i;
            continue;
        }
        size_t start = raw[i].pos;  // склеиваем подряд идущие ERR (байты одного символа UTF-8)
        std::string bad;
        while (i < raw.size() && raw[i].type == "ERR" && raw[i].pos == start + bad.size()) bad += raw[i++].lexeme;
        for (size_t k = 0; k < bad.size();) {
            if (bad.compare(k, 3, kArrowUtf8) == 0) {
                addTok("OP_IMPLIES", kArrowUtf8, start + k);
                k += 3;
            } else {
                size_t j = k;
                while (j < bad.size() && bad.compare(j, 3, kArrowUtf8) != 0) ++j;
                lexError(bad.substr(k, j - k), start + k);
                k = j;
            }
        }
    }
    addTok("EOF", "", source.size());

    Parser parser(std::move(toks), res.errors);
    res.ast = parser.program();
    std::stable_sort(res.errors.begin(), res.errors.end(), [](const Diagnostic& a, const Diagnostic& b) {
        return a.line != b.line ? a.line < b.line : a.col < b.col;
    });
    return res;
}

// Диагностика в стиле компиляторов: позиция, текст, строка исходника и '^' под столбцом ошибки.
std::string formatDiagnostic(const std::string& source, const Diagnostic& d, const std::string& file) {
    std::string out = file + ":" + std::to_string(d.line) + ":" + std::to_string(d.col) + ": error: " + d.message + "\n";
    auto ls = lineStarts(source);
    if (d.line < 1 || static_cast<size_t>(d.line) > ls.size()) return out;
    size_t b = ls[d.line - 1];
    size_t e = static_cast<size_t>(d.line) < ls.size() ? ls[d.line] : source.size();
    std::string text = source.substr(b, e - b);
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
    std::string caret;
    for (int i = 0; i < d.col - 1; ++i) caret += (static_cast<size_t>(i) < text.size() && text[i] == '\t') ? '\t' : ' ';
    return out + "    " + text + "\n    " + caret + "^\n";
}

}
