#pragma once

#include "ir.hpp"
#include "sema.hpp"

namespace funnyir {

// Генерирует IR только для семантически корректного аннотированного AST.
ModuleIR generate(const funnyparse::Node& program, const funnysema::SemaResult& annotations);

}  // namespace funnyir
