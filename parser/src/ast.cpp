#include "ast.hpp"

namespace funnyparse {

const Node* Node::find(Kind k) const {
    for (const auto& c : kids)
        if (c->kind == k) return c.get();
    return nullptr;
}

const char* kindName(Kind k) {
    switch (k) {
        case Kind::Program: return "Program";
        case Kind::Function: return "Function";
        case Kind::Formula: return "Formula";
        case Kind::Params: return "Params";
        case Kind::Returns: return "Returns";
        case Kind::Locals: return "Locals";
        case Kind::VarDef: return "VarDef";
        case Kind::Requires: return "Requires";
        case Kind::Ensures: return "Ensures";
        case Kind::Invariant: return "Invariant";
        case Kind::Block: return "Block";
        case Kind::Assign: return "Assign";
        case Kind::ArrayAssign: return "ArrayAssign";
        case Kind::TupleAssign: return "TupleAssign";
        case Kind::Targets: return "Targets";
        case Kind::If: return "If";
        case Kind::While: return "While";
        case Kind::Assert: return "Assert";
        case Kind::Assume: return "Assume";
        case Kind::Number: return "Number";
        case Kind::Var: return "Var";
        case Kind::Neg: return "Neg";
        case Kind::Binary: return "Binary";
        case Kind::Call: return "Call";
        case Kind::Index: return "Index";
        case Kind::BoolConst: return "BoolConst";
        case Kind::Compare: return "Compare";
        case Kind::Not: return "Not";
        case Kind::And: return "And";
        case Kind::Or: return "Or";
        case Kind::Implies: return "Implies";
        case Kind::Forall: return "Forall";
        case Kind::Exists: return "Exists";
        case Kind::FormulaRef: return "FormulaRef";
    }
    return "?";
}

static void tree(const Node& n, int depth, std::string& out) {
    out.append(depth * 2, ' ');
    out += kindName(n.kind);
    if (!n.text.empty()) out += " " + n.text;
    if (!n.extra.empty()) out += ": " + n.extra;
    out += "\n";
    for (const auto& c : n.kids) tree(*c, depth + 1, out);
}

std::string dumpTree(const Node& n) {
    std::string out;
    tree(n, 0, out);
    return out;
}

static std::string jsonStr(const std::string& s) {
    std::string o = "\"";
    for (unsigned char c : s) {
        if (c == '"') o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else if (c == '\n') o += "\\n";
        else if (c == '\t') o += "\\t";
        else if (c == '\r') o += "\\r";
        else o += static_cast<char>(c);  // UTF-8 допустим в JSON как есть
    }
    return o + "\"";
}

static void json(const Node& n, int depth, std::string& out) {
    std::string pad(depth * 2, ' ');
    out += pad + "{\"kind\": " + jsonStr(kindName(n.kind));
    if (!n.text.empty()) out += ", \"text\": " + jsonStr(n.text);
    if (!n.extra.empty()) out += ", \"type\": " + jsonStr(n.extra);
    out += ", \"line\": " + std::to_string(n.line) + ", \"col\": " + std::to_string(n.col);
    if (n.kids.empty()) {
        out += "}";
        return;
    }
    out += ", \"children\": [\n";
    for (size_t i = 0; i < n.kids.size(); ++i) {
        json(*n.kids[i], depth + 1, out);
        out += i + 1 < n.kids.size() ? ",\n" : "\n";
    }
    out += pad + "]}";
}

std::string dumpJSON(const Node& n) {
    std::string out;
    json(n, 0, out);
    return out + "\n";
}

std::string dumpSexpr(const Node& n) {
    if (n.kind == Kind::Number || n.kind == Kind::Var) return n.text;
    std::string s = "(";
    if (n.kind == Kind::Binary || n.kind == Kind::Compare) s += n.text;  // оператор вместо имени вида
    else {
        s += kindName(n.kind);
        if (!n.text.empty()) s += " " + n.text;
        if (!n.extra.empty()) s += " " + n.extra;
    }
    for (const auto& c : n.kids) s += " " + dumpSexpr(*c);
    return s + ")";
}

}  // namespace funnyparse
