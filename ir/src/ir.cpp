#include "ir.hpp"

#include <algorithm>
#include <sstream>

namespace funnyir {

Operand Operand::imm(std::int32_t v) { return {OperandKind::Immediate, "", v}; }
Operand Operand::var(std::string n) { return {OperandKind::Variable, std::move(n), 0}; }
Operand Operand::temp(std::string n) { return {OperandKind::Temporary, std::move(n), 0}; }

bool Instruction::isTerminator() const { return op == Op::Jump || op == Op::Branch || op == Op::Return; }
bool Instruction::hasSideEffects() const {
    return op == Op::Store || op == Op::Call || op == Op::Jump || op == Op::Branch || op == Op::Assert ||
           op == Op::Assume || op == Op::Return || op == Op::Label;
}

const FunctionIR* ModuleIR::find(const std::string& name) const {
    for (const auto& fn : functions) if (fn.name == name) return &fn;
    return nullptr;
}

const char* opName(Op op) {
    switch (op) {
        case Op::Label: return "label";
        case Op::Copy: return "copy";
        case Op::Neg: return "neg";
        case Op::Add: return "add";
        case Op::Sub: return "sub";
        case Op::Mul: return "mul";
        case Op::Div: return "div";
        case Op::Eq: return "eq";
        case Op::Ne: return "ne";
        case Op::Lt: return "lt";
        case Op::Le: return "le";
        case Op::Gt: return "gt";
        case Op::Ge: return "ge";
        case Op::Not: return "not";
        case Op::And: return "and";
        case Op::Or: return "or";
        case Op::Load: return "load";
        case Op::Store: return "store";
        case Op::Call: return "call";
        case Op::Jump: return "jump";
        case Op::Branch: return "branch";
        case Op::Assert: return "assert";
        case Op::Assume: return "assume";
        case Op::Return: return "return";
    }
    return "?";
}

std::string operandText(const Operand& op) {
    return op.isImmediate() ? std::to_string(op.value) : op.name;
}

static std::string joinOperands(const std::vector<Operand>& xs, const char* sep = ", ") {
    std::string out;
    for (size_t i = 0; i < xs.size(); ++i) out += (i ? sep : "") + operandText(xs[i]);
    return out;
}

std::string instructionText(const Instruction& in) {
    if (in.op == Op::Label) return in.name + ":";
    if (in.op == Op::Jump) return "jump " + in.name;
    if (in.op == Op::Branch) return "branch " + operandText(in.args.at(0)) + ", " + in.name + ", " + in.alt;
    if (in.op == Op::Return) return "return" + (in.args.empty() ? "" : " " + joinOperands(in.args));
    if (in.op == Op::Store) return "store " + joinOperands(in.args);
    if (in.op == Op::Assert || in.op == Op::Assume) return std::string(opName(in.op)) + " " + joinOperands(in.args);
    std::string lhs = joinOperands(in.dst);
    if (!lhs.empty()) lhs += " = ";
    if (in.op == Op::Call) return lhs + "call " + in.name + "(" + joinOperands(in.args) + ")";
    return lhs + opName(in.op) + (in.args.empty() ? "" : " " + joinOperands(in.args));
}

std::string dumpText(const ModuleIR& module) {
    std::ostringstream out;
    for (size_t f = 0; f < module.functions.size(); ++f) {
        const auto& fn = module.functions[f];
        out << "function " << fn.name << "(";
        for (size_t i = 0; i < fn.params.size(); ++i)
            out << (i ? ", " : "") << fn.params[i] << ": " << fn.types.at(fn.params[i]);
        out << ") returns (";
        for (size_t i = 0; i < fn.returns.size(); ++i)
            out << (i ? ", " : "") << fn.returns[i] << ": " << fn.types.at(fn.returns[i]);
        out << ")\n";
        for (const auto& in : fn.code) {
            if (in.op == Op::Label) out << instructionText(in) << "\n";
            else out << "  " << instructionText(in) << "\n";
        }
        if (f + 1 < module.functions.size()) out << "\n";
    }
    return out.str();
}

static std::string quote(const std::string& s) {
    std::string out = "\"";
    for (unsigned char c : s) {
        if (c == '\"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else out += static_cast<char>(c);
    }
    return out + "\"";
}

std::string dumpJSON(const ModuleIR& module) {
    std::ostringstream out;
    out << "{\n  \"functions\": [\n";
    for (size_t f = 0; f < module.functions.size(); ++f) {
        const auto& fn = module.functions[f];
        out << "    {\"name\": " << quote(fn.name) << ", \"params\": [";
        for (size_t i = 0; i < fn.params.size(); ++i) out << (i ? ", " : "") << quote(fn.params[i]);
        out << "], \"returns\": [";
        for (size_t i = 0; i < fn.returns.size(); ++i) out << (i ? ", " : "") << quote(fn.returns[i]);
        out << "], \"types\": {";
        size_t typeIndex = 0;
        for (const auto& [name, type] : fn.types)
            out << (typeIndex++ ? ", " : "") << quote(name) << ": " << quote(type);
        out << "}, \"instructions\": [\n";
        for (size_t i = 0; i < fn.code.size(); ++i) {
            const auto& in = fn.code[i];
            out << "      {\"op\": " << quote(opName(in.op));
            if (!in.dst.empty()) {
                out << ", \"dst\": [";
                for (size_t j = 0; j < in.dst.size(); ++j) out << (j ? ", " : "") << quote(operandText(in.dst[j]));
                out << "]";
            }
            if (!in.args.empty()) {
                out << ", \"args\": [";
                for (size_t j = 0; j < in.args.size(); ++j) {
                    out << (j ? ", " : "") << (in.args[j].isImmediate() ? std::to_string(in.args[j].value) : quote(in.args[j].name));
                }
                out << "]";
            }
            if (!in.name.empty()) out << ", \"name\": " << quote(in.name);
            if (!in.alt.empty()) out << ", \"alt\": " << quote(in.alt);
            out << "}" << (i + 1 < fn.code.size() ? "," : "") << "\n";
        }
        out << "    ]}" << (f + 1 < module.functions.size() ? "," : "") << "\n";
    }
    out << "  ]\n}\n";
    return out.str();
}

static std::string dotEscape(std::string s) {
    std::string out;
    for (char c : s) {
        if (c == '\"' || c == '\\') out += '\\';
        if (c == '\n') out += "\\l";
        else out += c;
    }
    return out;
}

std::string cfgDot(const FunctionIR& fn, const CFG& cfg) {
    std::ostringstream out;
    out << "digraph CFG_" << fn.name << " {\n  node [shape=box, fontname=\"monospace\"];\n";
    for (const auto& b : cfg.blocks) {
        std::string body = "B" + std::to_string(b.id) + " " + b.label + "\\l";
        for (size_t i : b.instructions) body += "  " + instructionText(fn.code[i]) + "\\l";
        out << "  B" << b.id << " [label=\"" << dotEscape(body) << "\"];\n";
    }
    for (const auto& b : cfg.blocks) for (int s : b.successors) out << "  B" << b.id << " -> B" << s << ";\n";
    out << "}\n";
    return out.str();
}

std::string interferenceDot(const std::string& functionName, const InterferenceGraph& graph) {
    static const char* palette[] = {"lightblue", "lightgreen", "gold", "salmon", "plum", "khaki", "cyan"};
    std::ostringstream out;
    out << "graph Interference_" << functionName << " {\n  node [style=filled, fontname=\"monospace\"];\n";
    for (const auto& [v, adj] : graph.edges) {
        (void)adj;
        int c = graph.colors.count(v) ? graph.colors.at(v) : 0;
        out << "  \"" << v << "\" [label=\"" << v << " / R" << c << "\", fillcolor=\""
            << palette[static_cast<size_t>(c) % (sizeof(palette) / sizeof(*palette))] << "\"];\n";
    }
    for (const auto& [a, xs] : graph.edges) for (const auto& b : xs) if (a < b) out << "  \"" << a << "\" -- \"" << b << "\";\n";
    out << "}\n";
    return out.str();
}

Metrics metrics(const FunctionIR& fn, const CFG& cfg, const InterferenceGraph& graph) {
    Metrics m;
    m.instructions = fn.code.size();
    m.blocks = cfg.blocks.size();
    std::set<std::string> temps;
    for (const auto& in : fn.code) {
        for (const auto& x : in.dst) if (x.isTemporary()) temps.insert(x.name);
        for (const auto& x : in.args) if (x.isTemporary()) temps.insert(x.name);
    }
    m.temporaries = temps.size();
    for (const auto& [v, xs] : graph.edges) { (void)v; m.interferenceEdges += xs.size(); }
    m.interferenceEdges /= 2;
    m.colors = graph.colorCount;
    return m;
}

}  // namespace funnyir
