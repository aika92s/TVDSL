#include "sema.hpp"
#include <algorithm>

namespace funnysema {

using funnyparse::Diagnostic;
using funnyparse::Kind;
using funnyparse::Node;

const char* tyName(Ty t) {
    switch (t) {
        case Ty::Int: return "int";
        case Ty::IntArray: return "int[]";
        case Ty::Bool: return "bool";
        case Ty::Error: return "error";
        default: return "";
    }
}

const NodeInfo& SemaResult::at(const Node& n) const {
    static const NodeInfo empty;
    auto it = info.find(&n);
    return it == info.end() ? empty : it->second;
}

std::string typeString(const NodeInfo& i) {
    if (i.type != Ty::Tuple) return tyName(i.type);
    std::string s = "(";
    for (size_t k = 0; k < i.tuple.size(); ++k) s += (k ? "," : "") + std::string(tyName(i.tuple[k]));
    return s + ")";
}

namespace {

enum class Role { Param, Return, Local, Quant };

const char* roleSymbol(Role r) {
    switch (r) {
        case Role::Param: return "param";
        case Role::Return: return "return";
        case Role::Local: return "local";
        case Role::Quant: return "quantifier";
    }
    return "";
}
// Как роль называется в сообщениях.
const char* roleText(Role r) {
    switch (r) {
        case Role::Param: return "parameter";
        case Role::Return: return "return value";
        case Role::Local: return "local variable";
        case Role::Quant: return "quantifier variable";
    }
    return "";
}

struct VarSym {
    Ty type = Ty::Error;
    Role role = Role::Param;
    int line = 0, col = 0;
};

// Где мы сейчас находимся: от этого зависит, какие переменные видны (см. docs/types.md).
enum class Ctx { Pre, Post, Body, Formula };

Ty tyFromDecl(const std::string& s) { return s == "int[]" ? Ty::IntArray : Ty::Int; }

std::string pos(int line, int col) { return std::to_string(line) + ":" + std::to_string(col); }

// Первая позиция выражения в исходнике: у бинарных операций позиция узла — оператор, а ошибку типа
// удобнее показывать на начале выражения.
const Node& startNode(const Node& n) {
    const Node* p = &n;
    while ((p->kind == Kind::Binary || p->kind == Kind::Compare || p->kind == Kind::And || p->kind == Kind::Or ||
            p->kind == Kind::Implies) && !p->kids.empty())
        p = p->kids[0].get();
    return *p;
}

class Checker {
public:
    SemaResult res;

    void run(const Node& program) {
        // проход 1: таблица определений модуля (вызов функции, определённой ниже, — не ошибка)
        for (const auto& d : program.kids) registerDefinition(*d);
        // проход 2: проверка тел
        for (const auto& d : program.kids) {
            if (d->kind == Kind::Function) checkFunction(*d);
            else if (d->kind == Kind::Formula) checkFormula(*d);
        }
        std::stable_sort(res.errors.begin(), res.errors.end(), [](const Diagnostic& a, const Diagnostic& b) {
            return a.line != b.line ? a.line < b.line : a.col < b.col;
        });
    }

private:
    std::map<std::string, VarSym> vars_;                      // переменные текущего определения
    std::vector<std::pair<std::string, VarSym>> quants_;      // стек переменных кванторов
    Ctx ctx_ = Ctx::Body;

    void err(int line, int col, const std::string& msg) { res.errors.push_back({line, col, msg}); }
    void err(const Node& at, const std::string& msg) { err(at.line, at.col, msg); }
    void errAtStart(const Node& e, const std::string& msg) {
        const Node& s = startNode(e);
        err(s.line, s.col, msg);
    }

    Ty set(const Node& n, Ty t, const std::string& symbol = "") {
        NodeInfo& i = res.info[&n];
        i.type = t;
        if (!symbol.empty()) i.symbol = symbol;
        return t;
    }

    //определения верхнего уровня

    void registerDefinition(const Node& d) {
        Signature s;
        s.name = d.text;
        s.isFormula = d.kind == Kind::Formula;
        s.line = d.line;
        s.col = d.col;
        if (const Node* p = d.find(Kind::Params))
            for (const auto& v : p->kids) s.params.push_back(tyFromDecl(v->extra));
        if (const Node* r = d.find(Kind::Returns))
            for (const auto& v : r->kids) s.returns.push_back(tyFromDecl(v->extra));
        // запись сигнатуры в аннотацию Function / Formula
        res.info[&d].signature = signatureString(s);
        res.info[&d].symbol = s.isFormula ? "formula" : "function";

        auto it = res.functions.find(s.name);
        if (it != res.functions.end()) {
            err(d, "redefinition of '" + s.name + "': already defined as " + (it->second.isFormula ? "formula" : "function") +
                       " at " + pos(it->second.line, it->second.col) + " (names must be unique in a module, no overloads)");
            return;  // остаётся первое определение
        }
        res.functions[s.name] = s;
    }

    static std::string signatureString(const Signature& s) {
        std::string o = "(";
        for (size_t i = 0; i < s.params.size(); ++i) o += (i ? "," : "") + std::string(tyName(s.params[i]));
        o += ")->";
        if (s.isFormula) return o + "bool";
        o += "(";
        for (size_t i = 0; i < s.returns.size(); ++i) o += (i ? "," : "") + std::string(tyName(s.returns[i]));
        return o + ")";
    }

    // Объявляет переменные из списка Params / Returns / Locals; повторное имя — ошибка (остаётся первое объявление).
    void declareList(const Node* list, Role role) {
        if (!list) return;
        for (const auto& v : list->kids) {
            Ty t = tyFromDecl(v->extra);
            set(*v, t, roleSymbol(role));
            if (role == Role::Local && t == Ty::IntArray)
                err(*v, "local variable '" + v->text + "' cannot have type int[] (local variables can only be int)");
            auto it = vars_.find(v->text);
            if (it != vars_.end()) {
                err(*v, "duplicate declaration of '" + v->text + "': already declared as " + roleText(it->second.role) +
                            " at " + pos(it->second.line, it->second.col));
                continue;
            }
            vars_[v->text] = {t, role, v->line, v->col};
        }
    }

    void resetScope(Ctx c) {
        vars_.clear();
        quants_.clear();
        ctx_ = c;
    }

    void checkFunction(const Node& f) {
        resetScope(Ctx::Body);
        declareList(f.find(Kind::Params), Role::Param);
        declareList(f.find(Kind::Returns), Role::Return);
        declareList(f.find(Kind::Locals), Role::Local);

        if (const Node* r = f.find(Kind::Requires)) {
            ctx_ = Ctx::Pre;
            predicate(*r->kids[0]);
        }
        if (const Node* e = f.find(Kind::Ensures)) {
            ctx_ = Ctx::Post;
            predicate(*e->kids[0]);
        }
        ctx_ = Ctx::Body;
        statement(*f.kids.back());
    }

    void checkFormula(const Node& f) {
        resetScope(Ctx::Formula);
        declareList(f.find(Kind::Params), Role::Param);
        predicate(*f.kids.back());
    }

    // ---------- видимость переменных ----------

    bool visible(Role r) const {
        switch (ctx_) {
            case Ctx::Pre:
            case Ctx::Formula: return r == Role::Param;
            case Ctx::Post: return r == Role::Param || r == Role::Return;
            case Ctx::Body: return true;
        }
        return true;
    }
    const char* ctxText() const {
        switch (ctx_) {
            case Ctx::Pre: return "the precondition (requires), where only parameters are visible";
            case Ctx::Post: return "the postcondition (ensures), where only parameters and return values are visible";
            case Ctx::Formula: return "a formula body, where only the formula parameters are visible";
            case Ctx::Body: return "this scope";
        }
        return "";
    }

    // Ищет переменную (сначала кванторы, потом переменные определения). Ошибку выдаёт сам; nullptr — не найдена/не видна.
    const VarSym* lookup(const std::string& name, const Node& at) {
        for (auto it = quants_.rbegin(); it != quants_.rend(); ++it)
            if (it->first == name) return &it->second;
        auto it = vars_.find(name);
        if (it == vars_.end()) {
            err(at, "unknown variable '" + name + "'");
            return nullptr;
        }
        if (!visible(it->second.role)) {
            err(at, "variable '" + name + "' (" + roleText(it->second.role) + " declared at " +
                        pos(it->second.line, it->second.col) + ") is not visible in " + ctxText());
            return nullptr;
        }
        return &it->second;
    }

    //  выражения, условия, предикаты

    void predicate(const Node& n) {
        Ty t = expr(n);
        if (t != Ty::Bool && t != Ty::Error) errAtStart(n, std::string("expected a boolean condition, found ") + tyName(t));
    }

    // Результат вызова: типы возврата; false — вызов не удалось разрешить.
    bool resolveCall(const Node& n, bool formulaRef, std::vector<Ty>& rets) {
        // аргументы проверяются всегда: так аннотируются все узлы и находятся все ошибки внутри них
        std::vector<Ty> at;
        for (const auto& a : n.kids) at.push_back(expr(*a));

        std::vector<Ty> params;
        std::string what;  // "function" | "builtin" | "formula"
        if (n.text == "length") {
            params = {Ty::IntArray};
            rets = {Ty::Int};
            what = "builtin";
        } else {
            auto it = res.functions.find(n.text);
            if (it == res.functions.end()) {
                err(n, std::string("unknown ") + (formulaRef ? "formula" : "function") + " '" + n.text + "'");
                return false;
            }
            const Signature& s = it->second;
            if (formulaRef && !s.isFormula) {
                err(n, "'" + n.text + "' is a function, not a formula: a bare function call is not a predicate (compare its result, e.g. " +
                           n.text + "(...) > 0)");
                return false;
            }
            if (!formulaRef && s.isFormula) {
                err(n, "'" + n.text + "' is a formula: formulas can be used only in predicates (requires, ensures, invariant, assert, assume, formula bodies)");
                return false;
            }
            params = s.params;
            rets = s.returns;
            what = s.isFormula ? "formula" : "function";
        }
        res.info[&n].symbol = what;

        const std::string label = (what == "formula" ? "formula '" : what == "builtin" ? "built-in '" : "function '") + n.text + "'";
        if (at.size() != params.size()) {
            err(n, label + " expects " + std::to_string(params.size()) + " argument(s), found " + std::to_string(at.size()));
            return true;
        }
        for (size_t i = 0; i < at.size(); ++i)
            if (at[i] != Ty::Error && at[i] != params[i])
                errAtStart(*n.kids[i], "argument " + std::to_string(i + 1) + " of " + label + ": expected " + tyName(params[i]) +
                                           ", found " + tyName(at[i]));
        return true;
    }

    // Операнды арифметики и сравнений: оба int (или уже Error).
    void needInts(const Node& n, const std::string& opText, Ty a, Ty b, bool& bad) {
        bad = false;
        if ((a != Ty::Int && a != Ty::Error) || (b != Ty::Int && b != Ty::Error)) {
            err(n, "operator '" + opText + "' requires int operands, found " + tyName(a) + " and " + tyName(b));
            bad = true;
        }
    }

    Ty expr(const Node& n) {
        switch (n.kind) {
            case Kind::Number: return number(n, false);
            case Kind::Var: {
                const VarSym* v = lookup(n.text, n);
                return v ? set(n, v->type, roleSymbol(v->role)) : set(n, Ty::Error);
            }
            case Kind::Neg: {
                const Node& k = *n.kids[0];
                Ty t = k.kind == Kind::Number ? number(k, true) : expr(k);
                if (t != Ty::Int && t != Ty::Error) {
                    err(n, std::string("unary '-' requires an int operand, found ") + tyName(t));
                    return set(n, Ty::Error);
                }
                return set(n, t == Ty::Error ? Ty::Error : Ty::Int);
            }
            case Kind::Binary: {
                Ty a = expr(*n.kids[0]), b = expr(*n.kids[1]);
                bool bad;
                needInts(n, n.text, a, b, bad);
                return set(n, bad || a == Ty::Error || b == Ty::Error ? Ty::Error : Ty::Int);
            }
            case Kind::Call: {
                std::vector<Ty> rets;
                if (!resolveCall(n, false, rets)) return set(n, Ty::Error);
                if (rets.size() != 1) {
                    err(n, "function '" + n.text + "' returns " + std::to_string(rets.size()) +
                               " values and cannot be used in an expression (use a tuple assignment: a, b = " + n.text + "(...);)");
                    return set(n, Ty::Error);
                }
                return set(n, rets[0]);
            }
            case Kind::Index: {
                const VarSym* v = lookup(n.text, n);
                Ty idx = expr(*n.kids[0]);
                if (idx != Ty::Int && idx != Ty::Error)
                    errAtStart(*n.kids[0], std::string("array index must be int, found ") + tyName(idx));
                if (!v) return set(n, Ty::Error);
                res.info[&n].symbol = roleSymbol(v->role);
                if (v->type != Ty::IntArray) {
                    err(n, "'" + n.text + "' is not an array (its type is " + tyName(v->type) + "), it cannot be indexed");
                    return set(n, Ty::Error);
                }
                return set(n, Ty::Int, roleSymbol(v->role));
            }
            case Kind::BoolConst: return set(n, Ty::Bool);
            case Kind::Compare: {
                Ty a = expr(*n.kids[0]), b = expr(*n.kids[1]);
                bool bad;
                needInts(n, n.text, a, b, bad);
                return set(n, Ty::Bool);  // результат сравнения — bool в любом случае
            }
            case Kind::Not: {
                Ty a = expr(*n.kids[0]);
                if (a != Ty::Bool && a != Ty::Error) err(n, std::string("operator 'not' requires a boolean operand, found ") + tyName(a));
                return set(n, Ty::Bool);
            }
            case Kind::And:
            case Kind::Or:
            case Kind::Implies: {
                Ty a = expr(*n.kids[0]), b = expr(*n.kids[1]);
                const char* op = n.kind == Kind::And ? "and" : n.kind == Kind::Or ? "or" : "->";
                if ((a != Ty::Bool && a != Ty::Error) || (b != Ty::Bool && b != Ty::Error))
                    err(n, std::string("operator '") + op + "' requires boolean operands, found " + tyName(a) + " and " + tyName(b));
                return set(n, Ty::Bool);
            }
            case Kind::Forall:
            case Kind::Exists: return quantifier(n);
            case Kind::FormulaRef: {
                std::vector<Ty> rets;
                resolveCall(n, true, rets);
                return set(n, Ty::Bool);
            }
            default: return set(n, Ty::Error);
        }
    }

    // Число: 0..2147483647; 2147483648 допустимо только сразу под унарным минусом (это -2^31).
    Ty number(const Node& n, bool underNeg) {
        const std::string& s = n.text;
        bool tooBig = s.size() > 10 || (s.size() == 10 && s > (underNeg ? "2147483648" : "2147483647"));
        if (tooBig) {
            err(n, "integer literal " + s + " is out of range for int (32-bit signed: " +
                       (underNeg ? "-2147483648" : "0") + ".." + "2147483647)");
            return set(n, Ty::Error);
        }
        return set(n, Ty::Int);
    }

    // forall/exists (v: T | P): имя должно быть уникально в области, где стоит квантор.
    Ty quantifier(const Node& n) {
        Ty t = tyFromDecl(n.extra);
        auto outer = std::find_if(quants_.rbegin(), quants_.rend(), [&](const auto& q) { return q.first == n.text; });
        auto var = vars_.find(n.text);
        if (outer != quants_.rend())
            err(n, "quantifier variable '" + n.text + "' is already declared by an enclosing quantifier at " +
                       pos(outer->second.line, outer->second.col));
        else if (var != vars_.end() && visible(var->second.role))
            err(n, "quantifier variable '" + n.text + "' duplicates the " + roleText(var->second.role) + " '" + n.text + "' declared at " +
                       pos(var->second.line, var->second.col) + " (names in a scope must be unique)");
        quants_.push_back({n.text, {t, Role::Quant, n.line, n.col}});
        predicate(*n.kids[0]);
        quants_.pop_back();
        return set(n, Ty::Bool);
    }

    //  операторы

    // Цель присваивания: переменная должна существовать и не быть параметром. nullptr — не найдена.
    const VarSym* target(const std::string& name, const Node& at) {
        const VarSym* v = lookup(name, at);
        if (v && v->role == Role::Param)
            err(at, "cannot assign to parameter '" + name + "' (parameters are read-only)");
        return v;
    }

    void statement(const Node& n) {
        switch (n.kind) {
            case Kind::Block:
                for (const auto& k : n.kids) statement(*k);
                break;
            case Kind::Assign: {
                const VarSym* v = target(n.text, n);
                Ty rt = expr(*n.kids[0]);
                if (v) set(n, v->type, roleSymbol(v->role));
                else set(n, Ty::Error);
                if (v && rt != Ty::Error && rt != v->type)
                    errAtStart(*n.kids[0], "type mismatch in assignment to '" + n.text + "': expected " + tyName(v->type) + ", found " + tyName(rt));
                break;
            }
            case Kind::ArrayAssign: {
                const VarSym* v = lookup(n.text, n);
                Ty it = expr(*n.kids[0]), vt = expr(*n.kids[1]);
                if (it != Ty::Int && it != Ty::Error)
                    errAtStart(*n.kids[0], std::string("array index must be int, found ") + tyName(it));
                if (vt != Ty::Int && vt != Ty::Error)
                    errAtStart(*n.kids[1], std::string("array element must be int, found ") + tyName(vt));
                if (!v) { set(n, Ty::Error); break; }
                set(n, v->type, roleSymbol(v->role));
                if (v->type != Ty::IntArray)
                    err(n, "'" + n.text + "' is not an array (its type is " + tyName(v->type) + "), it cannot be indexed");
                else if (v->role == Role::Param)
                    err(n, "cannot modify array parameter '" + n.text + "' (parameters, including arrays, are read-only)");
                break;
            }
            case Kind::TupleAssign: tupleAssign(n); break;
            case Kind::If:
                predicate(*n.kids[0]);
                for (size_t i = 1; i < n.kids.size(); ++i) statement(*n.kids[i]);
                break;
            case Kind::While:
                predicate(*n.kids[0]);
                for (size_t i = 1; i < n.kids.size(); ++i) {
                    if (n.kids[i]->kind == Kind::Invariant) predicate(*n.kids[i]->kids[0]);
                    else statement(*n.kids[i]);
                }
                break;
            case Kind::Assert:
            case Kind::Assume: predicate(*n.kids[0]); break;
            default: break;
        }
    }

    // a, b = f(...);  число и типы целей должны совпасть с returns функции.
    void tupleAssign(const Node& n) {
        const Node& targets = *n.kids[0];
        const Node& call = *n.kids[1];
        std::vector<const VarSym*> syms;
        for (const auto& t : targets.kids) {
            const VarSym* v = target(t->text, *t);
            syms.push_back(v);
            if (v) set(*t, v->type, roleSymbol(v->role));
            else set(*t, Ty::Error);
        }
        std::vector<Ty> rets;
        bool resolved = call.kind == Kind::Call && resolveCall(call, false, rets);
        if (!resolved) { set(call, Ty::Error); return; }
        NodeInfo& ci = res.info[&call];
        ci.type = Ty::Tuple;
        ci.tuple = rets;
        if (rets.size() != targets.kids.size()) {
            err(n, "tuple assignment has " + std::to_string(targets.kids.size()) + " target(s), but '" + call.text + "' returns " +
                       std::to_string(rets.size()) + " value(s)");
            return;
        }
        for (size_t i = 0; i < rets.size(); ++i)
            if (syms[i] && syms[i]->type != rets[i])
                err(*targets.kids[i], "cannot assign value #" + std::to_string(i + 1) + " of '" + call.text + "' (type " + tyName(rets[i]) +
                                          ") to '" + targets.kids[i]->text + "' of type " + tyName(syms[i]->type));
    }
};

}  // namespace

SemaResult analyze(const Node& program) {
    Checker c;
    c.run(program);
    return std::move(c.res);
}

//  вывод аннотированного AST

static std::string jsonStr(const std::string& s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        if (c == '"') o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else if (c == '\t') o += "\\t";
        else if (c == '\r') o += "\\r";
        else o += static_cast<char>(c);
    }
    return o + "\"";
}

static void tree(const Node& n, const SemaResult& r, int depth, std::string& out) {
    const NodeInfo& i = r.at(n);
    out.append(depth * 2, ' ');
    out += funnyparse::kindName(n.kind);
    if (!n.text.empty()) out += " " + n.text;
    if (!n.extra.empty()) out += ": " + n.extra;
    std::string ts = typeString(i);
    if (!ts.empty() && n.kind != Kind::VarDef) out += " -> " + ts;  // у VarDef тип уже напечатан после имени
    if (!i.signature.empty()) out += "  " + i.signature;
    out += "\n";
    for (const auto& c : n.kids) tree(*c, r, depth + 1, out);
}

std::string dumpAnnotatedTree(const Node& n, const SemaResult& r) {
    std::string out;
    tree(n, r, 0, out);
    return out;
}

static void json(const Node& n, const SemaResult& r, int depth, std::string& out) {
    const NodeInfo& i = r.at(n);
    std::string pad(depth * 2, ' ');
    out += pad + "{\"kind\": " + jsonStr(funnyparse::kindName(n.kind));
    if (!n.text.empty()) out += ", \"text\": " + jsonStr(n.text);
    out += ", \"line\": " + std::to_string(n.line) + ", \"col\": " + std::to_string(n.col);
    std::string ts = typeString(i);
    if (!ts.empty()) out += ", \"type\": " + jsonStr(ts);
    if (!i.symbol.empty()) out += ", \"symbol\": " + jsonStr(i.symbol);
    if (!i.signature.empty()) out += ", \"signature\": " + jsonStr(i.signature);
    if (!n.extra.empty() && (n.kind == Kind::Forall || n.kind == Kind::Exists)) out += ", \"var_type\": " + jsonStr(n.extra);
    if (n.kids.empty()) {
        out += "}";
        return;
    }
    out += ", \"children\": [\n";
    for (size_t k = 0; k < n.kids.size(); ++k) {
        json(*n.kids[k], r, depth + 1, out);
        out += k + 1 < n.kids.size() ? ",\n" : "\n";
    }
    out += pad + "]}";
}

std::string dumpAnnotatedJSON(const Node& n, const SemaResult& r) {
    std::string out;
    json(n, r, 0, out);
    return out + "\n";
}

std::string dumpAnnotatedSexpr(const Node& n, const SemaResult& r) {
    const NodeInfo& i = r.at(n);
    std::string ts = typeString(i);
    std::string suffix = ts.empty() || n.kind == Kind::VarDef ? "" : ":" + ts;
    if (n.kind == Kind::Number || n.kind == Kind::Var) return n.text + suffix;
    std::string s = "(";
    if (n.kind == Kind::Binary || n.kind == Kind::Compare) s += n.text + suffix;
    else {
        s += funnyparse::kindName(n.kind) + suffix;
        if (!n.text.empty()) s += " " + n.text;
        if (!n.extra.empty()) s += " " + n.extra;
    }
    for (const auto& c : n.kids) s += " " + dumpAnnotatedSexpr(*c, r);
    return s + ")";
}

}  // namespace funnysema
