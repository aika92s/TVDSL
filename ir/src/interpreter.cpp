#include "interpreter.hpp"

#include <limits>
#include <map>
#include <stdexcept>

namespace funnyir {
namespace {

class Runner {
public:
    Runner(const ModuleIR& module, size_t limit) : module_(module), remaining_(limit) {}

    std::vector<Value> call(const std::string& name, const std::vector<Value>& arguments) {
        const FunctionIR* fn = module_.find(name);
        if (!fn) throw std::runtime_error("IR call to unknown function '" + name + "'");
        if (arguments.size() != fn->params.size()) throw std::runtime_error("IR call argument count mismatch for '" + name + "'");
        std::map<std::string, Value> env;
        for (size_t i = 0; i < arguments.size(); ++i) env[fn->params[i]] = arguments[i];
        for (const auto& v : fn->returns) env[v] = std::int32_t{0};
        for (const auto& v : fn->locals) env[v] = std::int32_t{0};
        std::map<std::string, size_t> labels;
        for (size_t i = 0; i < fn->code.size(); ++i) if (fn->code[i].op == Op::Label) labels[fn->code[i].name] = i;

        auto get = [&](const Operand& op) -> Value {
            if (op.isImmediate()) return op.value;
            auto it = env.find(op.name);
            if (it == env.end()) throw std::runtime_error("read of unknown IR value '" + op.name + "'");
            return it->second;
        };
        auto integer = [&](const Operand& op) {
            Value v = get(op);
            if (!std::holds_alternative<std::int32_t>(v)) throw std::runtime_error("expected integer IR operand");
            return std::get<std::int32_t>(v);
        };
        auto array = [&](const Operand& op) {
            Value v = get(op);
            if (!std::holds_alternative<Array>(v) || !std::get<Array>(v)) throw std::runtime_error("expected array IR operand");
            return std::get<Array>(v);
        };
        auto jump = [&](const std::string& label) {
            auto it = labels.find(label);
            if (it == labels.end()) throw std::runtime_error("IR jump to unknown label '" + label + "'");
            return it->second;
        };
        auto wrapNeg = [](std::int32_t a) { return static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(a)); };
        auto wrap2 = [](std::int32_t a, std::int32_t b, Op op) {
            std::uint32_t x = static_cast<std::uint32_t>(a), y = static_cast<std::uint32_t>(b);
            if (op == Op::Add) return static_cast<std::int32_t>(x + y);
            if (op == Op::Sub) return static_cast<std::int32_t>(x - y);
            return static_cast<std::int32_t>(x * y);
        };

        size_t pc = 0;
        while (pc < fn->code.size()) {
            if (remaining_-- == 0) throw std::runtime_error("IR interpreter step limit exceeded");
            const Instruction& in = fn->code[pc];
            auto assignOne = [&](Value value) {
                if (in.dst.size() != 1) throw std::runtime_error("IR instruction destination count mismatch");
                env[in.dst[0].name] = std::move(value);
            };
            switch (in.op) {
                case Op::Label: ++pc; break;
                case Op::Copy: assignOne(get(in.args.at(0))); ++pc; break;
                case Op::Neg: assignOne(wrapNeg(integer(in.args.at(0)))); ++pc; break;
                case Op::Not: assignOne(std::int32_t{integer(in.args.at(0)) == 0}); ++pc; break;
                case Op::Add: case Op::Sub: case Op::Mul:
                    assignOne(wrap2(integer(in.args.at(0)), integer(in.args.at(1)), in.op)); ++pc; break;
                case Op::Div: {
                    std::int32_t a = integer(in.args.at(0)), b = integer(in.args.at(1));
                    if (b == 0 || (a == std::numeric_limits<std::int32_t>::min() && b == -1))
                        throw std::runtime_error("integer division trap");
                    assignOne(a / b); ++pc; break;
                }
                case Op::Eq: case Op::Ne: case Op::Lt: case Op::Le: case Op::Gt: case Op::Ge: {
                    std::int32_t a = integer(in.args.at(0)), b = integer(in.args.at(1));
                    bool v = in.op == Op::Eq ? a == b : in.op == Op::Ne ? a != b : in.op == Op::Lt ? a < b :
                             in.op == Op::Le ? a <= b : in.op == Op::Gt ? a > b : a >= b;
                    assignOne(std::int32_t{v}); ++pc; break;
                }
                case Op::And: case Op::Or: {
                    bool a = integer(in.args.at(0)) != 0, b = integer(in.args.at(1)) != 0;
                    assignOne(std::int32_t{in.op == Op::And ? a && b : a || b}); ++pc; break;
                }
                case Op::Load: {
                    Array a = array(in.args.at(0));
                    std::int32_t i = integer(in.args.at(1));
                    if (i < 0 || static_cast<size_t>(i) >= a->size()) throw std::runtime_error("array index out of bounds");
                    assignOne(a->at(static_cast<size_t>(i))); ++pc; break;
                }
                case Op::Store: {
                    Array a = array(in.args.at(0));
                    std::int32_t i = integer(in.args.at(1));
                    if (i < 0 || static_cast<size_t>(i) >= a->size()) throw std::runtime_error("array index out of bounds");
                    a->at(static_cast<size_t>(i)) = integer(in.args.at(2)); ++pc; break;
                }
                case Op::Call: {
                    std::vector<Value> args;
                    for (const auto& a : in.args) args.push_back(get(a));
                    std::vector<Value> result;
                    if (in.name == "length") {
                        if (args.size() != 1 || !std::holds_alternative<Array>(args[0])) throw std::runtime_error("invalid length call");
                        result.push_back(static_cast<std::int32_t>(std::get<Array>(args[0])->size()));
                    } else if (in.name.rfind("@formula:", 0) == 0) {
                        throw std::runtime_error("verification formula cannot be executed");
                    } else result = call(in.name, args);
                    if (result.size() != in.dst.size()) throw std::runtime_error("IR call result count mismatch for '" + in.name + "'");
                    for (size_t i = 0; i < result.size(); ++i) env[in.dst[i].name] = result[i];
                    ++pc; break;
                }
                case Op::Jump: pc = jump(in.name); break;
                case Op::Branch: pc = jump(integer(in.args.at(0)) != 0 ? in.name : in.alt); break;
                case Op::Assert:
                    if (integer(in.args.at(0)) == 0) throw std::runtime_error("assertion failed");
                    ++pc; break;
                case Op::Assume:
                    if (integer(in.args.at(0)) == 0) throw std::runtime_error("assumption violated");
                    ++pc; break;
                case Op::Return: {
                    std::vector<Value> result;
                    for (const auto& a : in.args) result.push_back(get(a));
                    return result;
                }
            }
        }
        throw std::runtime_error("IR function ended without return");
    }

private:
    const ModuleIR& module_;
    size_t remaining_;
};

}  // namespace

std::vector<Value> execute(const ModuleIR& module, const std::string& function,
                           const std::vector<Value>& arguments, size_t stepLimit) {
    return Runner(module, stepLimit).call(function, arguments);
}

}  // namespace funnyir
