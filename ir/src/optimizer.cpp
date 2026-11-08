#include "optimizer.hpp"

#include <algorithm>
#include <limits>
#include <map>

#include "analysis.hpp"

namespace funnyir {
namespace {

std::string key(const Operand& x) { return x.isImmediate() ? "" : x.name; }

bool fold(Op op, const std::vector<Operand>& a, std::int32_t& result) {
    auto u = [](std::int32_t v) { return static_cast<std::uint32_t>(v); };
    auto s = [](std::uint32_t v) { return static_cast<std::int32_t>(v); };
    if ((op == Op::Neg || op == Op::Not) && a.size() == 1 && a[0].isImmediate()) {
        result = op == Op::Neg ? s(0u - u(a[0].value)) : (a[0].value == 0 ? 1 : 0);
        return true;
    }
    if (a.size() != 2 || !a[0].isImmediate() || !a[1].isImmediate()) return false;
    const std::int32_t x = a[0].value, y = a[1].value;
    switch (op) {
        case Op::Add: result = s(u(x) + u(y)); return true;
        case Op::Sub: result = s(u(x) - u(y)); return true;
        case Op::Mul: result = s(u(x) * u(y)); return true;
        case Op::Div:
            if (y == 0 || (x == std::numeric_limits<std::int32_t>::min() && y == -1)) return false;
            result = x / y; return true;
        case Op::Eq: result = x == y; return true;
        case Op::Ne: result = x != y; return true;
        case Op::Lt: result = x < y; return true;
        case Op::Le: result = x <= y; return true;
        case Op::Gt: result = x > y; return true;
        case Op::Ge: result = x >= y; return true;
        case Op::And: result = (x != 0) && (y != 0); return true;
        case Op::Or: result = (x != 0) || (y != 0); return true;
        default: return false;
    }
}

bool simplify(Instruction& in) {
    if (in.args.size() != 2 || in.dst.size() != 1) return false;
    const auto& a = in.args[0];
    const auto& b = in.args[1];
    auto copy = [&](Operand x) {
        in.op = Op::Copy;
        in.args = {std::move(x)};
        return true;
    };
    auto is = [](const Operand& x, std::int32_t v) { return x.isImmediate() && x.value == v; };
    switch (in.op) {
        case Op::Add:
            if (is(a, 0)) return copy(b);
            if (is(b, 0)) return copy(a);
            break;
        case Op::Sub:
            if (is(b, 0)) return copy(a);
            break;
        case Op::Mul:
            if (is(a, 0) || is(b, 0)) return copy(Operand::imm(0));
            if (is(a, 1)) return copy(b);
            if (is(b, 1)) return copy(a);
            break;
        case Op::Div:
            if (is(b, 1)) return copy(a);
            break;
        case Op::And:
            if (is(a, 0) || is(b, 0)) return copy(Operand::imm(0));
            if (is(a, 1)) return copy(b);
            if (is(b, 1)) return copy(a);
            break;
        case Op::Or:
            if (is(a, 1) || is(b, 1)) return copy(Operand::imm(1));
            if (is(a, 0)) return copy(b);
            if (is(b, 0)) return copy(a);
            break;
        default: break;
    }
    return false;
}

void localRewrite(FunctionIR& fn, const OptimizationOptions& options) {
    CFG cfg = buildCFG(fn);
    for (const auto& block : cfg.blocks) {
        std::map<std::string, std::int32_t> constants;
        for (size_t index : block.instructions) {
            Instruction& in = fn.code[index];
            if (options.constantPropagation) {
                for (auto& a : in.args) {
                    auto it = constants.find(key(a));
                    if (!a.isImmediate() && it != constants.end()) a = Operand::imm(it->second);
                }
                std::int32_t value = 0;
                if (fold(in.op, in.args, value) && in.dst.size() == 1) {
                    in.op = Op::Copy;
                    in.args = {Operand::imm(value)};
                }
            }
            if (options.algebraicSimplification) simplify(in);

            for (const auto& d : in.dst) {
                if (d.isImmediate()) continue;
                if (in.op == Op::Copy && in.args.size() == 1 && in.args[0].isImmediate()) constants[d.name] = in.args[0].value;
                else constants.erase(d.name);
            }
            if (in.op == Op::Call) {
                // A call may observe and modify arrays, but scalar names have value semantics.
                for (const auto& d : in.dst) if (!d.isImmediate()) constants.erase(d.name);
            }
        }
    }
}

bool removable(const Instruction& in) {
    switch (in.op) {
        case Op::Copy: case Op::Neg: case Op::Add: case Op::Sub: case Op::Mul:
        case Op::Eq: case Op::Ne: case Op::Lt: case Op::Le: case Op::Gt: case Op::Ge:
        case Op::Not: case Op::And: case Op::Or:
            return !in.dst.empty() && std::all_of(in.dst.begin(), in.dst.end(), [](const Operand& x) { return x.isTemporary(); });
        default:
            return false;
    }
}

void dce(FunctionIR& fn) {
    bool changed;
    do {
        changed = false;
        CFG cfg = buildCFG(fn);
        computeLiveness(fn, cfg);
        std::vector<bool> remove(fn.code.size(), false);
        for (const auto& block : cfg.blocks) {
            std::set<std::string> live = block.liveOut;
            for (auto it = block.instructions.rbegin(); it != block.instructions.rend(); ++it) {
                const Instruction& in = fn.code[*it];
                bool allDead = !in.dst.empty();
                for (const auto& d : in.dst) if (!d.isTemporary() || live.count(d.name)) allDead = false;
                if (allDead && removable(in)) {
                    remove[*it] = true;
                    changed = true;
                    continue;
                }
                for (const auto& d : in.dst) if (d.isTemporary()) live.erase(d.name);
                for (const auto& a : in.args) if (a.isTemporary()) live.insert(a.name);
            }
        }
        if (changed) {
            std::vector<Instruction> kept;
            kept.reserve(fn.code.size());
            for (size_t i = 0; i < fn.code.size(); ++i) if (!remove[i]) kept.push_back(std::move(fn.code[i]));
            fn.code = std::move(kept);
        }
    } while (changed);
}

}  // namespace

FunctionIR optimize(const FunctionIR& input, const OptimizationOptions& options) {
    FunctionIR result = input;
    if (options.constantPropagation || options.algebraicSimplification) localRewrite(result, options);
    if (options.deadCodeElimination) dce(result);
    std::set<std::string> present;
    for (const auto& in : result.code) {
        for (const auto& x : in.dst) if (!x.isImmediate()) present.insert(x.name);
        for (const auto& x : in.args) if (!x.isImmediate()) present.insert(x.name);
    }
    for (auto it = result.types.begin(); it != result.types.end();) {
        if (!it->first.empty() && it->first[0] == '%' && !present.count(it->first)) it = result.types.erase(it);
        else ++it;
    }
    return result;
}

ModuleIR optimize(const ModuleIR& input, const OptimizationOptions& options) {
    ModuleIR result;
    for (const auto& fn : input.functions) result.functions.push_back(optimize(fn, options));
    return result;
}

}  // namespace funnyir
