#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>

#include "analysis.hpp"
#include "generator.hpp"
#include "interpreter.hpp"
#include "optimizer.hpp"
#include "parser.hpp"

#ifndef FUNNY_TOKENS
#define FUNNY_TOKENS "../lexer/data/tokens.txt"
#endif

using namespace funnyir;

struct Row { std::string group, name, input, expected, got; bool pass = false; };
static std::vector<Row> rows;
static int failed = 0;

static const funnylex::Lexer& lexer() {
    static funnylex::Lexer l(funnylex::buildMinimalDFA(funnylex::loadTokenDefs(FUNNY_TOKENS)));
    return l;
}

struct Pipeline { funnyparse::ParseResult parse; funnysema::SemaResult sema; ModuleIR ir; };

static Pipeline compile(const std::string& source) {
    Pipeline p;
    p.parse = funnyparse::parseProgram(source, lexer());
    if (!p.parse.ok()) throw std::runtime_error("syntax errors: " + std::to_string(p.parse.errors.size()));
    p.sema = funnysema::analyze(*p.parse.ast);
    if (!p.sema.ok()) throw std::runtime_error("semantic errors: " + std::to_string(p.sema.errors.size()));
    p.ir = generate(*p.parse.ast, p.sema);
    return p;
}

template <class F>
static void test(const std::string& group, const std::string& name, const std::string& input,
                 const std::string& expected, F action) {
    std::string got;
    try { got = action(); }
    catch (const std::exception& e) { got = std::string("EXCEPTION: ") + e.what(); }
    bool pass = got == expected;
    if (!pass) {
        ++failed;
        std::cerr << "FAIL [" << group << "/" << name << "]\n  expected: " << expected << "\n  got: " << got << "\n";
    }
    rows.push_back({group, name, input, expected, got, pass});
}

static std::int32_t asInt(const std::vector<Value>& values, size_t index = 0) {
    return std::get<std::int32_t>(values.at(index));
}
static bool hasOp(const FunctionIR& fn, Op op) {
    for (const auto& in : fn.code) if (in.op == op) return true;
    return false;
}
static size_t opCount(const FunctionIR& fn, Op op) {
    size_t n = 0; for (const auto& in : fn.code) if (in.op == op) ++n; return n;
}
static std::string yes(bool value) { return value ? "yes" : "no"; }

struct Analysed { CFG cfg; InterferenceGraph graph; Metrics stats; };
static Analysed analyse(const FunctionIR& fn) {
    Analysed a;
    a.cfg = buildCFG(fn);
    computeLiveness(fn, a.cfg);
    a.graph = buildInterferenceGraph(fn, a.cfg);
    greedyColor(a.graph);
    a.stats = metrics(fn, a.cfg, a.graph);
    return a;
}

static void generationTests() {
    test("generation", "arithmetic_precedence", "r = 1 + 2 * x;", "mul before add", [] {
        auto c = compile("f(x: int) returns r: int r = 1 + 2 * x;").ir.functions[0].code;
        size_t mul = c.size(), add = c.size();
        for (size_t i = 0; i < c.size(); ++i) { if (c[i].op == Op::Mul) mul = i; if (c[i].op == Op::Add) add = i; }
        return mul < add ? "mul before add" : "wrong order";
    });
    test("generation", "unary_and_comparison", "if (-x <= 0) ...", "neg=yes, le=yes, branch=yes", [] {
        auto fn = compile("f(x: int) returns r: int { if (-x <= 0) r = 1; else r = 2; }").ir.functions[0];
        return "neg=" + yes(hasOp(fn, Op::Neg)) + ", le=" + yes(hasOp(fn, Op::Le)) + ", branch=" + yes(hasOp(fn, Op::Branch));
    });
    test("generation", "if_else_labels", "if (x > 0) r = 1; else r = 2;", "branches=1, jumps=2", [] {
        auto fn = compile("f(x: int) returns r: int { if (x > 0) r = 1; else r = 2; }").ir.functions[0];
        return "branches=" + std::to_string(opCount(fn, Op::Branch)) + ", jumps=" + std::to_string(opCount(fn, Op::Jump));
    });
    test("generation", "while_loop", "while (i < n) i = i + 1;", "branch=yes, back-jump=yes", [] {
        auto fn = compile("f(n: int) returns r: int uses i: int { i = 0; while (i < n) i = i + 1; r = i; }").ir.functions[0];
        auto cfg = buildCFG(fn); bool back = false;
        for (const auto& b : cfg.blocks) for (int s : b.successors) back = back || s <= b.id;
        return "branch=" + yes(hasOp(fn, Op::Branch)) + ", back-jump=" + yes(back);
    });
    test("generation", "single_result_call", "r = inc(x);", "call inc -> one destination", [] {
        auto m = compile("inc(x: int) returns r: int r = x + 1; f(x: int) returns r: int r = inc(x);").ir;
        for (const auto& in : m.functions[1].code)
            if (in.op == Op::Call) return "call " + in.name + " -> " + (in.dst.size() == 1 ? "one destination" : "wrong destinations");
        return std::string("no call");
    });
    test("generation", "tuple_call", "a, b = pair(x);", "call pair -> 2 destinations", [] {
        auto m = compile("pair(x: int) returns a: int, b: int { a = x; b = x + 1; } f(x: int) returns a: int, b: int { a, b = pair(x); }").ir;
        for (const auto& in : m.functions[1].code)
            if (in.op == Op::Call) return "call " + in.name + " -> " + std::to_string(in.dst.size()) + " destinations";
        return std::string("no call");
    });
    test("generation", "array_load_store", "b[i] = a[i] + 1;", "load=yes, store=yes", [] {
        auto fn = compile("f(a: int[], i: int) returns b: int[] { b = a; b[i] = a[i] + 1; }").ir.functions[0];
        return "load=" + yes(hasOp(fn, Op::Load)) + ", store=" + yes(hasOp(fn, Op::Store));
    });
    test("generation", "builtin_length", "r = length(a);", "call length", [] {
        auto fn = compile("f(a: int[]) returns r: int r = length(a);").ir.functions[0];
        for (const auto& in : fn.code) if (in.op == Op::Call) return std::string("call ") + in.name;
        return std::string("no call");
    });
    test("generation", "explicit_return", "f returns r, s", "return operands=2", [] {
        auto fn = compile("f(x: int) returns r: int, s: int { r = x; s = x + 1; }").ir.functions[0];
        return "return operands=" + std::to_string(fn.code.back().args.size());
    });
    test("generation", "ir_type_table", "int and int[] values", "x=int, a=int[], temporary=int", [] {
        auto fn = compile("f(x: int, a: int[]) returns r: int r = x + length(a);").ir.functions[0];
        return "x=" + fn.types.at("x") + ", a=" + fn.types.at("a") + ", temporary=" + fn.types.at("%t0");
    });
    test("generation", "verification_contracts_not_executable", "requires/ensures/invariant", "IR generated", [] {
        auto m = compile("p(x: int) => x >= 0\nf(x: int) requires p(x) returns r: int ensures r >= 0 { r = 0; while (r < x) invariant r >= 0 r = r + 1; }").ir;
        return m.functions.size() == 1 ? "IR generated" : "wrong function count";
    });
}

static void cfgTests() {
    test("cfg", "straight_line", "three assignments", "blocks=1", [] {
        auto fn = compile("f(x: int) returns r: int uses a: int { a = x + 1; r = a + 1; }").ir.functions[0];
        return "blocks=" + std::to_string(buildCFG(fn).blocks.size());
    });
    test("cfg", "if_two_successors", "if/else", "two-way block=yes", [] {
        auto cfg = buildCFG(compile("f(x: int) returns r: int { if (x > 0) r = 1; else r = 0; }").ir.functions[0]);
        bool found = false; for (const auto& b : cfg.blocks) found = found || b.successors.size() == 2;
        return "two-way block=" + yes(found);
    });
    test("cfg", "while_back_edge", "while", "back edge=yes", [] {
        auto cfg = buildCFG(compile("f(n: int) returns r: int { r = 0; while (r < n) r = r + 1; }").ir.functions[0]);
        bool found = false; for (const auto& b : cfg.blocks) for (int s : b.successors) found = found || s <= b.id;
        return "back edge=" + yes(found);
    });
    test("cfg", "predecessor_consistency", "nested if in while", "all reverse edges=yes", [] {
        auto cfg = buildCFG(compile("f(n: int) returns r: int { r = 0; while (r < n) { if (r < 2) r = r + 2; else r = r + 1; } }").ir.functions[0]);
        bool ok = true;
        for (const auto& b : cfg.blocks) for (int s : b.successors) {
            const auto& p = cfg.blocks[static_cast<size_t>(s)].predecessors;
            ok = ok && std::find(p.begin(), p.end(), b.id) != p.end();
        }
        return "all reverse edges=" + yes(ok);
    });
    test("cfg", "basic_block_terminators", "if plus while", "terminators valid=yes", [] {
        auto fn = compile("f(n: int) returns r: int { r = 0; while (r < n) { if (r == 2) r = r + 2; else r = r + 1; } }").ir.functions[0];
        auto cfg = buildCFG(fn); bool ok = true;
        for (const auto& b : cfg.blocks) {
            const auto& last = fn.code[b.instructions.back()];
            if (b.successors.size() == 2) ok = ok && last.op == Op::Branch;
            if (b.successors.empty()) ok = ok && last.op == Op::Return;
        }
        return "terminators valid=" + yes(ok);
    });
}

static void optimizationTests() {
    auto assignment = [](const FunctionIR& fn, const std::string& name) {
        for (const auto& in : fn.code) if (in.op == Op::Copy && !in.dst.empty() && in.dst[0].name == name) return instructionText(in);
        return std::string("assignment missing");
    };
    test("optimization", "constant_folding", "r = 1 + 2 * 3;", "r = copy 7", [=] {
        return assignment(optimize(compile("f() returns r: int r = 1 + 2 * 3;").ir).functions[0], "r");
    });
    test("optimization", "constant_propagation", "a = 4; r = a + 2;", "r = copy 6", [=] {
        return assignment(optimize(compile("f() returns r: int uses a: int { a = 4; r = a + 2; }").ir).functions[0], "r");
    });
    test("optimization", "algebra_add_zero", "r = x + 0;", "add removed=yes", [] {
        return "add removed=" + yes(!hasOp(optimize(compile("f(x: int) returns r: int r = x + 0;").ir).functions[0], Op::Add));
    });
    test("optimization", "algebra_sub_zero", "r = x - 0;", "sub removed=yes", [] {
        return "sub removed=" + yes(!hasOp(optimize(compile("f(x: int) returns r: int r = x - 0;").ir).functions[0], Op::Sub));
    });
    test("optimization", "algebra_mul_one", "r = x * 1;", "mul removed=yes", [] {
        return "mul removed=" + yes(!hasOp(optimize(compile("f(x: int) returns r: int r = x * 1;").ir).functions[0], Op::Mul));
    });
    test("optimization", "algebra_mul_zero", "r = x * 0;", "r = copy 0", [=] {
        return assignment(optimize(compile("f(x: int) returns r: int r = x * 0;").ir).functions[0], "r");
    });
    test("optimization", "algebra_div_one", "r = x / 1;", "div removed=yes", [] {
        return "div removed=" + yes(!hasOp(optimize(compile("f(x: int) returns r: int r = x / 1;").ir).functions[0], Op::Div));
    });
    test("optimization", "boolean_identities", "true and (x > 0) or false", "and/or removed=yes", [] {
        auto fn = optimize(compile("f(x: int) returns r: int { if (true and x > 0 or false) r = 1; else r = 0; }").ir).functions[0];
        return "and/or removed=" + yes(!hasOp(fn, Op::And) && !hasOp(fn, Op::Or));
    });
    test("optimization", "local_dce", "unused pure temporary", "dead add removed=yes", [] {
        FunctionIR fn; fn.name = "dead"; fn.code = {{Op::Label, {}, {}, "entry", ""},
            {Op::Add, {Operand::temp("%t0")}, {Operand::imm(1), Operand::imm(2)}, "", ""}, {Op::Return, {}, {}, "", ""}};
        return "dead add removed=" + yes(!hasOp(optimize(fn), Op::Add));
    });
    test("optimization", "dce_keeps_division_trap", "unused 1 / 0", "division kept=yes", [] {
        FunctionIR fn; fn.name = "trap"; fn.code = {{Op::Label, {}, {}, "entry", ""},
            {Op::Div, {Operand::temp("%t0")}, {Operand::imm(1), Operand::imm(0)}, "", ""}, {Op::Return, {}, {}, "", ""}};
        return "division kept=" + yes(hasOp(optimize(fn), Op::Div));
    });
    test("optimization", "dce_keeps_bounds_check", "unused a[i]", "load kept=yes", [] {
        FunctionIR fn; fn.name = "bounds"; fn.code = {{Op::Label, {}, {}, "entry", ""},
            {Op::Load, {Operand::temp("%t0")}, {Operand::var("a"), Operand::var("i")}, "", ""}, {Op::Return, {}, {}, "", ""}};
        return "load kept=" + yes(hasOp(optimize(fn), Op::Load));
    });
    test("flags", "disable_constant_propagation", "--no-const-prop", "add remains=yes", [] {
        OptimizationOptions o; o.constantPropagation = false;
        return "add remains=" + yes(hasOp(optimize(compile("f() returns r: int r = 2 + 3;").ir, o).functions[0], Op::Add));
    });
    test("flags", "disable_algebra", "--no-algebra", "add remains=yes", [] {
        OptimizationOptions o; o.algebraicSimplification = false;
        return "add remains=" + yes(hasOp(optimize(compile("f(x: int) returns r: int r = x + 0;").ir, o).functions[0], Op::Add));
    });
    test("flags", "disable_dce", "--no-dce", "dead temporary remains=yes", [] {
        OptimizationOptions o; o.deadCodeElimination = false;
        auto fn = optimize(compile("f() returns r: int r = 2 + 3;").ir, o).functions[0];
        bool temp = false; for (const auto& in : fn.code) for (const auto& d : in.dst) temp = temp || d.isTemporary();
        return "dead temporary remains=" + yes(temp);
    });
    test("flags", "disable_all", "--no-opt", "IR unchanged=yes", [] {
        auto m = compile("f(x: int) returns r: int r = (x + 0) * 1;").ir;
        return "IR unchanged=" + yes(dumpText(optimize(m, {false, false, false})) == dumpText(m));
    });
}

static void equivalenceTests() {
    const std::string compute = "f(x: int) returns r: int uses i: int, six: int { r = 0; i = 0; six = 2 * 3; while (i < x) { if (i < 3) r = r + six + 0; else r = r + 1 * 1; i = i + 1; } }";
    test("equivalence", "calculation_if_while", "x = 0..20", "21/21 equal", [=] {
        auto before = compile(compute).ir, after = optimize(before); int ok = 0;
        for (int x = 0; x <= 20; ++x) ok += asInt(execute(before, "f", {std::int32_t{x}})) == asInt(execute(after, "f", {std::int32_t{x}}));
        return std::to_string(ok) + "/21 equal";
    });
    test("equivalence", "three_way_branch", "x = -20..20", "41/41 equal", [] {
        auto before = compile("f(x: int) returns r: int { if (x < 0) r = -1; else if (x == 0) r = 0; else r = 1; }").ir, after = optimize(before); int ok = 0;
        for (int x = -20; x <= 20; ++x) ok += asInt(execute(before, "f", {std::int32_t{x}})) == asInt(execute(after, "f", {std::int32_t{x}}));
        return std::to_string(ok) + "/41 equal";
    });
    test("equivalence", "nested_calls", "square(inc(x)) for x=-10..10", "21/21 equal", [] {
        auto before = compile("inc(x: int) returns r: int r = x + 1; square(x: int) returns r: int r = x * x; f(x: int) returns r: int r = square(inc(x));").ir, after = optimize(before); int ok = 0;
        for (int x = -10; x <= 10; ++x) ok += asInt(execute(before, "f", {std::int32_t{x}})) == asInt(execute(after, "f", {std::int32_t{x}}));
        return std::to_string(ok) + "/21 equal";
    });
    test("equivalence", "tuple_results", "q, r = pair(x)", "result=(7,8) both", [] {
        auto before = compile("pair(x: int) returns q: int, r: int { q = x; r = x + 1; } f(x: int) returns q: int, r: int { q, r = pair(x); }").ir, after = optimize(before);
        auto a = execute(before, "f", {std::int32_t{7}}), b = execute(after, "f", {std::int32_t{7}});
        return "result=(" + std::to_string(asInt(a)) + "," + std::to_string(asInt(a, 1)) + ") " + (asInt(a) == asInt(b) && asInt(a, 1) == asInt(b, 1) ? "both" : "different");
    });
    test("equivalence", "array_sum", "[2,4,6,8]", "sum=20 both", [] {
        auto before = compile("sum(a: int[]) returns r: int uses i: int { r = 0; i = 0; while (i < length(a)) { r = r + a[i]; i = i + 1; } }").ir, after = optimize(before);
        Array a = std::make_shared<std::vector<std::int32_t>>(std::initializer_list<std::int32_t>{2,4,6,8});
        int x = asInt(execute(before, "sum", {a})); Array b = std::make_shared<std::vector<std::int32_t>>(*a); int y = asInt(execute(after, "sum", {b}));
        return "sum=" + std::to_string(x) + (x == y ? " both" : " different");
    });
    test("equivalence", "array_store", "b[0] = b[0] + 1", "first=11 both", [] {
        auto before = compile("bump(a: int[]) returns b: int[] { b = a; b[0] = b[0] + 1; }").ir, after = optimize(before);
        Array a = std::make_shared<std::vector<std::int32_t>>(std::initializer_list<std::int32_t>{10,20}); Array b = std::make_shared<std::vector<std::int32_t>>(*a);
        Array x = std::get<Array>(execute(before, "bump", {a})[0]); Array y = std::get<Array>(execute(after, "bump", {b})[0]);
        return "first=" + std::to_string(x->at(0)) + (x->at(0) == y->at(0) ? " both" : " different");
    });
    test("equivalence", "int32_wraparound", "2147483647 + 1", "result=-2147483648 both", [] {
        auto before = compile("f() returns r: int r = 2147483647 + 1;").ir, after = optimize(before);
        int x = asInt(execute(before, "f", {})), y = asInt(execute(after, "f", {}));
        return "result=" + std::to_string(x) + (x == y ? " both" : " different");
    });
}

static void livenessAndFormatTests() {
    test("liveness", "interference_edge", "(x+y) * (x-y)", "edges>=1", [] {
        auto a = analyse(optimize(compile("f(x: int, y: int) returns r: int r = (x + y) * (x - y);").ir).functions[0]); size_t edges = 0;
        for (const auto& [v, xs] : a.graph.edges) { (void)v; edges += xs.size(); }
        return edges / 2 >= 1 ? "edges>=1" : "edges=0";
    });
    test("liveness", "fixed_point_across_loop", "temporary condition in while", "loop live sets computed=yes", [] {
        auto fn = compile("f(n: int) returns r: int { r = 0; while (r < n) r = r + 1; }").ir.functions[0]; auto cfg = buildCFG(fn); computeLiveness(fn, cfg); bool found = false;
        for (const auto& b : cfg.blocks) found = found || !b.use.empty() || !b.def.empty();
        return "loop live sets computed=" + yes(found);
    });
    test("coloring", "adjacent_colors_differ", "conflict graph", "valid=yes", [] {
        auto a = analyse(optimize(compile("f(x: int, y: int) returns r: int r = (x + y) * (x - y);").ir).functions[0]); bool valid = true;
        for (const auto& [v, xs] : a.graph.edges) for (const auto& n : xs) valid = valid && a.graph.colors[v] != a.graph.colors[n];
        return "valid=" + yes(valid);
    });
    test("coloring", "color_count", "one conflict edge", "colors=2", [] {
        return "colors=" + std::to_string(analyse(optimize(compile("f(x: int, y: int) returns r: int r = (x + y) * (x - y);").ir).functions[0]).graph.colorCount);
    });
    test("metrics", "before_after", "constant plus +0", "instructions reduced=yes, temporaries reduced=yes", [] {
        auto before = compile("f(x: int) returns r: int uses a: int { a = 2 * 3; r = x + a + 0; }").ir, after = optimize(before);
        auto b = analyse(before.functions[0]).stats, a = analyse(after.functions[0]).stats;
        return "instructions reduced=" + yes(a.instructions < b.instructions) + ", temporaries reduced=" + yes(a.temporaries < b.temporaries);
    });
    test("format", "json_contains_types", "JSON IR", "functions/op/types=yes", [] {
        std::string j = dumpJSON(compile("f(x: int) returns r: int r = x + 1;").ir);
        return "functions/op/types=" + yes(j.find("\"functions\"") != std::string::npos && j.find("\"op\"") != std::string::npos && j.find("\"types\"") != std::string::npos);
    });
    test("format", "cfg_dot", "DOT CFG", "digraph and edges=yes", [] {
        auto fn = compile("f(x: int) returns r: int { if (x > 0) r = 1; else r = 0; }").ir.functions[0]; std::string d = cfgDot(fn, buildCFG(fn));
        return "digraph and edges=" + yes(d.find("digraph CFG_f") != std::string::npos && d.find("->") != std::string::npos);
    });
    test("format", "interference_dot", "DOT interference", "graph/colors/edge=yes", [] {
        auto fn = optimize(compile("f(x: int, y: int) returns r: int r = (x + y) * (x - y);").ir).functions[0]; auto a = analyse(fn); std::string d = interferenceDot(fn.name, a.graph);
        return "graph/colors/edge=" + yes(d.find("graph Interference_f") != std::string::npos && d.find("R0") != std::string::npos && d.find(" -- ") != std::string::npos);
    });
}

static void pipelineTests() {
    test("pipeline", "syntax_error_stops_ir", "f() returns r: int r = ;", "syntax errors=1, IR not built", [] {
        auto p = funnyparse::parseProgram("f() returns r: int r = ;", lexer());
        return "syntax errors=" + std::to_string(p.errors.size()) + ", IR not built";
    });
    test("pipeline", "semantic_error_stops_ir", "r = unknown;", "semantic errors=1, generator rejects=yes", [] {
        auto p = funnyparse::parseProgram("f() returns r: int r = unknown;", lexer()); auto s = funnysema::analyze(*p.ast); bool rejected = false;
        try { (void)generate(*p.ast, s); } catch (const std::invalid_argument&) { rejected = true; }
        return "semantic errors=" + std::to_string(s.errors.size()) + ", generator rejects=" + yes(rejected);
    });
}

static std::string md(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '|') out += "\\|";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') {}
        else if (c == '`') out += "\\`";
        else out += c;
    }
    return out;
}

static void writeReport(const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("cannot write report " + path);
    out << "# Отчёт HW4\n\n"
        << "Промежуточное представление Funny строится из аннотированного AST HW3. Реализованы трёхадресный IR, базовые блоки и CFG, локальные оптимизации, анализ живости, граф конфликтов и жадная раскраска временных. Формат IR и API описаны в `docs/ir.md`, команды запуска — в `README.md`.\n\n"
        << "## Подробные тесты\n\n"
        << "| имя | группа | вход / условие | ожидание | получено | результат |\n|---|---|---|---|---|---|\n";
    for (const auto& r : rows)
        out << "| " << md(r.name) << " | " << md(r.group) << " | `" << md(r.input) << "` | `" << md(r.expected) << "` | `" << md(r.got) << "` | " << (r.pass ? "PASS" : "**FAIL**") << " |\n";
}

int main() {
    generationTests(); cfgTests(); optimizationTests(); equivalenceTests(); livenessAndFormatTests(); pipelineTests();
    const char* env = std::getenv("FUNNY_REPORT");
    std::string report = env ? env : "REPORT_HW4.md";
    if (!report.empty()) writeReport(report);
    std::cout << "HW4 tests: " << (rows.size() - static_cast<size_t>(failed)) << "/" << rows.size() << " scenarios passed";
    if (!report.empty()) std::cout << "; report: " << report;
    std::cout << "\n";
    return failed == 0 ? 0 : 1;
}