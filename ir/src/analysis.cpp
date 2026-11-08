#include "analysis.hpp"

#include <algorithm>
#include <stdexcept>

namespace funnyir {
namespace {

void addUnique(std::vector<int>& xs, int v) {
    if (std::find(xs.begin(), xs.end(), v) == xs.end()) xs.push_back(v);
}

std::set<std::string> tempUses(const Instruction& in) {
    std::set<std::string> out;
    for (const auto& x : in.args) if (x.isTemporary()) out.insert(x.name);
    return out;
}

std::set<std::string> tempDefs(const Instruction& in) {
    std::set<std::string> out;
    for (const auto& x : in.dst) if (x.isTemporary()) out.insert(x.name);
    return out;
}

void addEdge(InterferenceGraph& graph, const std::string& a, const std::string& b) {
    graph.edges[a];
    graph.edges[b];
    if (a == b) return;
    graph.edges[a].insert(b);
    graph.edges[b].insert(a);
}

}  // namespace

CFG buildCFG(const FunctionIR& fn) {
    CFG cfg;
    if (fn.code.empty()) return cfg;
    std::set<size_t> leaders{0};
    for (size_t i = 0; i < fn.code.size(); ++i) {
        if (fn.code[i].op == Op::Label) leaders.insert(i);
        if (fn.code[i].isTerminator() && i + 1 < fn.code.size()) leaders.insert(i + 1);
    }
    std::vector<size_t> starts(leaders.begin(), leaders.end());
    for (size_t b = 0; b < starts.size(); ++b) {
        BasicBlock block;
        block.id = static_cast<int>(b);
        const size_t begin = starts[b], end = b + 1 < starts.size() ? starts[b + 1] : fn.code.size();
        block.label = fn.code[begin].op == Op::Label ? fn.code[begin].name : "B" + std::to_string(b);
        for (size_t i = begin; i < end; ++i) {
            block.instructions.push_back(i);
            if (fn.code[i].op == Op::Label) cfg.labelToBlock[fn.code[i].name] = block.id;
        }
        cfg.blocks.push_back(std::move(block));
    }

    auto target = [&](const std::string& name) {
        auto it = cfg.labelToBlock.find(name);
        if (it == cfg.labelToBlock.end()) throw std::runtime_error("IR jump to unknown label '" + name + "'");
        return it->second;
    };
    for (size_t b = 0; b < cfg.blocks.size(); ++b) {
        auto& block = cfg.blocks[b];
        const Instruction& last = fn.code[block.instructions.back()];
        if (last.op == Op::Jump) addUnique(block.successors, target(last.name));
        else if (last.op == Op::Branch) {
            addUnique(block.successors, target(last.name));
            addUnique(block.successors, target(last.alt));
        } else if (last.op != Op::Return && b + 1 < cfg.blocks.size()) {
            addUnique(block.successors, static_cast<int>(b + 1));
        }
    }
    for (const auto& block : cfg.blocks)
        for (int successor : block.successors) addUnique(cfg.blocks[static_cast<size_t>(successor)].predecessors, block.id);
    return cfg;
}

void computeLiveness(const FunctionIR& fn, CFG& cfg) {
    for (auto& block : cfg.blocks) {
        block.use.clear(); block.def.clear(); block.liveIn.clear(); block.liveOut.clear();
        for (size_t i : block.instructions) {
            for (const auto& u : tempUses(fn.code[i])) if (!block.def.count(u)) block.use.insert(u);
            for (const auto& d : tempDefs(fn.code[i])) block.def.insert(d);
        }
    }
    bool changed;
    do {
        changed = false;
        for (auto it = cfg.blocks.rbegin(); it != cfg.blocks.rend(); ++it) {
            std::set<std::string> out;
            for (int s : it->successors) out.insert(cfg.blocks[static_cast<size_t>(s)].liveIn.begin(),
                                                    cfg.blocks[static_cast<size_t>(s)].liveIn.end());
            std::set<std::string> in = it->use;
            for (const auto& v : out) if (!it->def.count(v)) in.insert(v);
            if (in != it->liveIn || out != it->liveOut) {
                it->liveIn = std::move(in);
                it->liveOut = std::move(out);
                changed = true;
            }
        }
    } while (changed);
}

InterferenceGraph buildInterferenceGraph(const FunctionIR& fn, const CFG& cfg) {
    InterferenceGraph graph;
    for (const auto& in : fn.code) {
        for (const auto& x : in.args) if (x.isTemporary()) graph.edges[x.name];
        for (const auto& x : in.dst) if (x.isTemporary()) graph.edges[x.name];
    }
    for (const auto& block : cfg.blocks) {
        std::set<std::string> live = block.liveOut;
        for (auto it = block.instructions.rbegin(); it != block.instructions.rend(); ++it) {
            const auto defs = tempDefs(fn.code[*it]);
            for (const auto& d : defs) for (const auto& v : live) addEdge(graph, d, v);
            for (auto a = defs.begin(); a != defs.end(); ++a)
                for (auto b = std::next(a); b != defs.end(); ++b) addEdge(graph, *a, *b);
            for (const auto& d : defs) live.erase(d);
            const auto uses = tempUses(fn.code[*it]);
            live.insert(uses.begin(), uses.end());
        }
    }
    return graph;
}

void greedyColor(InterferenceGraph& graph) {
    std::vector<std::string> order;
    for (const auto& [v, adj] : graph.edges) { (void)adj; order.push_back(v); }
    std::sort(order.begin(), order.end(), [&](const std::string& a, const std::string& b) {
        if (graph.edges[a].size() != graph.edges[b].size()) return graph.edges[a].size() > graph.edges[b].size();
        return a < b;
    });
    graph.colors.clear();
    graph.colorCount = 0;
    for (const auto& v : order) {
        std::set<int> unavailable;
        for (const auto& n : graph.edges[v]) if (graph.colors.count(n)) unavailable.insert(graph.colors[n]);
        int color = 0;
        while (unavailable.count(color)) ++color;
        graph.colors[v] = color;
        graph.colorCount = std::max(graph.colorCount, color + 1);
    }
}

}  // namespace funnyir
