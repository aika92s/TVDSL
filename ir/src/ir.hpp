#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace funnyir {

enum class OperandKind { Immediate, Variable, Temporary };

struct Operand {
    OperandKind kind = OperandKind::Immediate;
    std::string name;
    std::int32_t value = 0;

    static Operand imm(std::int32_t v);
    static Operand var(std::string n);
    static Operand temp(std::string n);
    bool isImmediate() const { return kind == OperandKind::Immediate; }
    bool isTemporary() const { return kind == OperandKind::Temporary; }
};

enum class Op {
    Label, Copy, Neg, Add, Sub, Mul, Div,
    Eq, Ne, Lt, Le, Gt, Ge, Not, And, Or,
    Load, Store, Call, Jump, Branch, Assert, Assume, Return
};

struct Instruction {
    Op op = Op::Copy;
    std::vector<Operand> dst;
    std::vector<Operand> args;
    std::string name;  // label / jump target / callee
    std::string alt;   // false target for Branch

    bool isTerminator() const;
    bool hasSideEffects() const;
};

struct FunctionIR {
    std::string name;
    std::vector<std::string> params;
    std::vector<std::string> returns;
    std::vector<std::string> locals;
    std::map<std::string, std::string> types;  // имя -> int | int[] | bool (bool хранится как JVM int)
    std::vector<Instruction> code;
};

struct ModuleIR {
    std::vector<FunctionIR> functions;
    const FunctionIR* find(const std::string& name) const;
};

struct BasicBlock {
    int id = 0;
    std::string label;
    std::vector<size_t> instructions;
    std::vector<int> successors;
    std::vector<int> predecessors;
    std::set<std::string> use, def, liveIn, liveOut;
};

struct CFG {
    std::vector<BasicBlock> blocks;
    std::map<std::string, int> labelToBlock;
};

struct InterferenceGraph {
    std::map<std::string, std::set<std::string>> edges;
    std::map<std::string, int> colors;
    int colorCount = 0;
};

struct Metrics {
    size_t instructions = 0;
    size_t temporaries = 0;
    size_t blocks = 0;
    size_t interferenceEdges = 0;
    int colors = 0;
};

const char* opName(Op op);
std::string operandText(const Operand& op);
std::string instructionText(const Instruction& in);
std::string dumpText(const ModuleIR& module);
std::string dumpJSON(const ModuleIR& module);
std::string cfgDot(const FunctionIR& fn, const CFG& cfg);
std::string interferenceDot(const std::string& functionName, const InterferenceGraph& graph);
Metrics metrics(const FunctionIR& fn, const CFG& cfg, const InterferenceGraph& graph);

}  // namespace funnyir
