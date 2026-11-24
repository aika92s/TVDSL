#pragma once

#include "ir.hpp"

namespace funnyir {

CFG buildCFG(const FunctionIR& fn);
void computeLiveness(const FunctionIR& fn, CFG& cfg);
InterferenceGraph buildInterferenceGraph(const FunctionIR& fn, const CFG& cfg);
void greedyColor(InterferenceGraph& graph);

}  // namespace funnyir
