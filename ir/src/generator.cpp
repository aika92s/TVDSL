#include "generator.hpp"

#include <stdexcept>

namespace funnyir {
namespace {

using funnyparse::Kind;
using funnyparse::Node;

class Generator {
public:
    explicit Generator(const funnysema::SemaResult& annotations) : annotations_(annotations) {}

    ModuleIR run(const Node& program) {
        ModuleIR module;
        for (const auto& def : program.kids) {
            if (def->kind == Kind::Function) module.functions.push_back(function(*def));
        }
        return module;
    }

private:
    const funnysema::SemaResult& annotations_;
    FunctionIR* fn_ = nullptr;
    int nextTemp_ = 0;
    int nextLabel_ = 0;

    Operand temp(const Node& origin) {
        Operand result = Operand::temp("%t" + std::to_string(nextTemp_++));
        std::string type = funnysema::typeString(annotations_.at(origin));
        fn_->types[result.name] = type.empty() ? "int" : type;
        return result;
    }
    std::string label(const std::string& prefix) { return prefix + std::to_string(nextLabel_++); }
    void emit(Instruction in) { fn_->code.push_back(std::move(in)); }
    void emitLabel(const std::string& name) { emit({Op::Label, {}, {}, name, ""}); }
    void emitJump(const std::string& target) { emit({Op::Jump, {}, {}, target, ""}); }

    static void names(const Node* list, std::vector<std::string>& out, std::map<std::string, std::string>& types) {
        if (!list) return;
        for (const auto& v : list->kids) {
            out.push_back(v->text);
            types[v->text] = v->extra;
        }
    }

    FunctionIR function(const Node& n) {
        FunctionIR f;
        f.name = n.text;
        names(n.find(Kind::Params), f.params, f.types);
        names(n.find(Kind::Returns), f.returns, f.types);
        names(n.find(Kind::Locals), f.locals, f.types);
        fn_ = &f;
        nextTemp_ = 0;
        nextLabel_ = 0;
        emitLabel("entry");
        statement(*n.kids.back());
        std::vector<Operand> returns;
        for (const auto& name : f.returns) returns.push_back(Operand::var(name));
        emit({Op::Return, {}, std::move(returns), "", ""});
        fn_ = nullptr;
        return f;
    }

    static Op binaryOp(const std::string& op) {
        if (op == "+") return Op::Add;
        if (op == "-") return Op::Sub;
        if (op == "*") return Op::Mul;
        if (op == "/") return Op::Div;
        throw std::runtime_error("unknown binary operator '" + op + "'");
    }

    static Op compareOp(const std::string& op) {
        if (op == "==") return Op::Eq;
        if (op == "!=") return Op::Ne;
        if (op == "<") return Op::Lt;
        if (op == "<=") return Op::Le;
        if (op == ">") return Op::Gt;
        if (op == ">=") return Op::Ge;
        throw std::runtime_error("unknown comparison operator '" + op + "'");
    }

    Operand unary(Op op, Operand a, const Node& origin) {
        Operand d = temp(origin);
        emit({op, {d}, {std::move(a)}, "", ""});
        return d;
    }

    Operand binary(Op op, Operand a, Operand b, const Node& origin) {
        Operand d = temp(origin);
        emit({op, {d}, {std::move(a), std::move(b)}, "", ""});
        return d;
    }

    Operand expression(const Node& n) {
        switch (n.kind) {
            case Kind::Number: {
                std::uint32_t bits = static_cast<std::uint32_t>(std::stoull(n.text));
                return Operand::imm(static_cast<std::int32_t>(bits));
            }
            case Kind::Var:
                return Operand::var(n.text);
            case Kind::Neg:
                return unary(Op::Neg, expression(*n.kids[0]), n);
            case Kind::Binary: {
                Operand left = expression(*n.kids[0]);
                Operand right = expression(*n.kids[1]);
                return binary(binaryOp(n.text), left, right, n);
            }
            case Kind::Compare: {
                Operand left = expression(*n.kids[0]);
                Operand right = expression(*n.kids[1]);
                return binary(compareOp(n.text), left, right, n);
            }
            case Kind::Not:
                return unary(Op::Not, expression(*n.kids[0]), n);
            case Kind::And: {
                Operand left = expression(*n.kids[0]);
                Operand right = expression(*n.kids[1]);
                return binary(Op::And, left, right, n);
            }
            case Kind::Or: {
                Operand left = expression(*n.kids[0]);
                Operand right = expression(*n.kids[1]);
                return binary(Op::Or, left, right, n);
            }
            case Kind::Implies: {
                Operand left = unary(Op::Not, expression(*n.kids[0]), n);
                return binary(Op::Or, left, expression(*n.kids[1]), n);
            }
            case Kind::Index: {
                Operand d = temp(n);
                emit({Op::Load, {d}, {Operand::var(n.text), expression(*n.kids[0])}, "", ""});
                return d;
            }
            case Kind::Call:
            case Kind::FormulaRef: {
                std::vector<Operand> args;
                for (const auto& a : n.kids) args.push_back(expression(*a));
                Operand d = temp(n);
                emit({Op::Call, {d}, std::move(args), n.kind == Kind::FormulaRef ? "@formula:" + n.text : n.text, ""});
                return d;
            }
            case Kind::BoolConst:
                return Operand::imm(n.text == "true" ? 1 : 0);
            case Kind::Forall:
            case Kind::Exists:
                throw std::runtime_error("quantifiers are verification-only and cannot occur in executable IR");
            default:
                throw std::runtime_error(std::string("cannot generate expression IR for ") + funnyparse::kindName(n.kind));
        }
    }

    void statement(const Node& n) {
        switch (n.kind) {
            case Kind::Block:
                for (const auto& s : n.kids) statement(*s);
                return;
            case Kind::Assign:
                emit({Op::Copy, {Operand::var(n.text)}, {expression(*n.kids[0])}, "", ""});
                return;
            case Kind::ArrayAssign:
                emit({Op::Store, {}, {Operand::var(n.text), expression(*n.kids[0]), expression(*n.kids[1])}, "", ""});
                return;
            case Kind::TupleAssign: {
                const Node& targets = *n.kids[0];
                const Node& call = *n.kids[1];
                std::vector<Operand> dst, args;
                for (const auto& t : targets.kids) dst.push_back(Operand::var(t->text));
                for (const auto& a : call.kids) args.push_back(expression(*a));
                emit({Op::Call, std::move(dst), std::move(args), call.text, ""});
                return;
            }
            case Kind::If: {
                std::string thenLabel = label("if.then."), elseLabel = label("if.else."), endLabel = label("if.end.");
                Operand cond = expression(*n.kids[0]);
                emit({Op::Branch, {}, {cond}, thenLabel, n.kids.size() == 3 ? elseLabel : endLabel});
                emitLabel(thenLabel);
                statement(*n.kids[1]);
                emitJump(endLabel);
                if (n.kids.size() == 3) {
                    emitLabel(elseLabel);
                    statement(*n.kids[2]);
                    emitJump(endLabel);
                }
                emitLabel(endLabel);
                return;
            }
            case Kind::While: {
                std::string head = label("while.head."), body = label("while.body."), end = label("while.end.");
                emitJump(head);
                emitLabel(head);
                Operand cond = expression(*n.kids[0]);
                emit({Op::Branch, {}, {cond}, body, end});
                emitLabel(body);
                statement(*n.kids.back());
                emitJump(head);
                emitLabel(end);
                return;
            }
            case Kind::Assert:
                emit({Op::Assert, {}, {expression(*n.kids[0])}, "", ""});
                return;
            case Kind::Assume:
                emit({Op::Assume, {}, {expression(*n.kids[0])}, "", ""});
                return;
            default:
                throw std::runtime_error(std::string("cannot generate statement IR for ") + funnyparse::kindName(n.kind));
        }
    }
};

}  // namespace

ModuleIR generate(const funnyparse::Node& program, const funnysema::SemaResult& annotations) {
    if (!annotations.ok()) throw std::invalid_argument("IR generation requires a semantically valid annotated AST");
    return Generator(annotations).run(program);
}

}  // namespace funnyir
