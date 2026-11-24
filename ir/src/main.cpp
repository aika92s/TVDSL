#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

#include "analysis.hpp"
#include "generator.hpp"
#include "optimizer.hpp"
#include "parser.hpp"

#ifndef FUNNY_TOKENS
#define FUNNY_TOKENS "../lexer/data/tokens.txt"
#endif

using namespace funnyir;

static bool readAll(const std::string& path, std::string& out) {
    if (path == "-") { out.assign(std::istreambuf_iterator<char>(std::cin), {}); return true; }
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), {});
    return true;
}

static void writeAll(const std::string& path, const std::string& text) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("cannot write " + path);
    out << text;
}

struct Analysed {
    CFG cfg;
    InterferenceGraph graph;
    Metrics stats;
};

static Analysed analyse(const FunctionIR& fn) {
    Analysed a;
    a.cfg = buildCFG(fn);
    computeLiveness(fn, a.cfg);
    a.graph = buildInterferenceGraph(fn, a.cfg);
    greedyColor(a.graph);
    a.stats = metrics(fn, a.cfg, a.graph);
    return a;
}

int main(int argc, char** argv) {
    std::string tokens = FUNNY_TOKENS, input, cfgPath, graphPath, graphFunction;
    enum { Text, Json } format = Text;
    OptimizationOptions options;
    const char* usage =
        "usage: funny_ir [--tokens FILE] [--format text|json] [--no-opt] [--no-const-prop]\n"
        "                [--no-algebra] [--no-dce] [--function NAME]\n"
        "                [--cfg-dot FILE] [--interference-dot FILE] <file | ->\n";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--tokens" && i + 1 < argc) tokens = argv[++i];
        else if (a == "--format" && i + 1 < argc) {
            std::string f = argv[++i];
            if (f == "text") format = Text;
            else if (f == "json") format = Json;
            else { std::cerr << usage; return 2; }
        } else if (a == "--no-opt") options = {false, false, false};
        else if (a == "--no-const-prop") options.constantPropagation = false;
        else if (a == "--no-algebra") options.algebraicSimplification = false;
        else if (a == "--no-dce") options.deadCodeElimination = false;
        else if (a == "--cfg-dot" && i + 1 < argc) cfgPath = argv[++i];
        else if (a == "--interference-dot" && i + 1 < argc) graphPath = argv[++i];
        else if (a == "--function" && i + 1 < argc) graphFunction = argv[++i];
        else if (input.empty() && (a == "-" || a.empty() || a[0] != '-')) input = a;
        else { std::cerr << usage; return 2; }
    }
    if (input.empty()) { std::cerr << usage; return 2; }

    try {
        funnylex::Lexer lexer(funnylex::buildMinimalDFA(funnylex::loadTokenDefs(tokens)));
        std::string source;
        if (!readAll(input, source)) { std::cerr << "cannot open " << input << "\n"; return 2; }
        const std::string file = input == "-" ? "<stdin>" : input;
        funnyparse::ParseResult parsed = funnyparse::parseProgram(source, lexer);
        if (!parsed.ok()) {
            for (const auto& d : parsed.errors) std::cerr << funnyparse::formatDiagnostic(source, d, file);
            return 1;
        }
        funnysema::SemaResult checked = funnysema::analyze(*parsed.ast);
        if (!checked.ok()) {
            for (const auto& d : checked.errors) std::cerr << funnyparse::formatDiagnostic(source, d, file);
            return 1;
        }
        ModuleIR before = generate(*parsed.ast, checked);
        ModuleIR after = optimize(before, options);

        if (format == Text) {
            std::cout << "=== IR before optimization ===\n" << dumpText(before)
                      << "\n=== IR after optimization ===\n" << dumpText(after)
                      << "\n=== Metrics ===\n";
        } else {
            std::cout << "{\n\"before\": " << dumpJSON(before) << ",\n\"after\": " << dumpJSON(after) << ",\n\"metrics\": [\n";
        }
        for (size_t i = 0; i < after.functions.size(); ++i) {
            const FunctionIR& post = after.functions[i];
            const FunctionIR* pre = before.find(post.name);
            Analysed a0 = analyse(*pre), a1 = analyse(post);
            if (format == Text) {
                std::cout << post.name << ": instructions " << a0.stats.instructions << " -> " << a1.stats.instructions
                          << ", temporaries " << a0.stats.temporaries << " -> " << a1.stats.temporaries
                          << ", blocks " << a1.stats.blocks << ", colors " << a1.stats.colors << "\n";
            } else {
                std::cout << "  {\"function\": \"" << post.name << "\", \"instructions_before\": " << a0.stats.instructions
                          << ", \"instructions_after\": " << a1.stats.instructions << ", \"temporaries_before\": "
                          << a0.stats.temporaries << ", \"temporaries_after\": " << a1.stats.temporaries
                          << ", \"blocks\": " << a1.stats.blocks << ", \"colors\": " << a1.stats.colors << "}"
                          << (i + 1 < after.functions.size() ? "," : "") << "\n";
            }
        }
        if (format == Json) std::cout << "]\n}\n";

        if (!cfgPath.empty() || !graphPath.empty()) {
            const FunctionIR* selected = graphFunction.empty() ? (after.functions.empty() ? nullptr : &after.functions[0])
                                                                : after.find(graphFunction);
            if (!selected) throw std::runtime_error("cannot select function for DOT output");
            Analysed a = analyse(*selected);
            if (!cfgPath.empty()) writeAll(cfgPath, cfgDot(*selected, a.cfg));
            if (!graphPath.empty()) writeAll(graphPath, interferenceDot(selected->name, a.graph));
        }
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 2;
    }
}
