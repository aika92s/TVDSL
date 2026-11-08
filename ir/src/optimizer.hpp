#pragma once

#include "ir.hpp"

namespace funnyir {

struct OptimizationOptions {
    bool constantPropagation = true;
    bool algebraicSimplification = true;
    bool deadCodeElimination = true;
};

FunctionIR optimize(const FunctionIR& input, const OptimizationOptions& options = {});
ModuleIR optimize(const ModuleIR& input, const OptimizationOptions& options = {});

}  // namespace funnyir
