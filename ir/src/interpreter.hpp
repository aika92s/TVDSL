#pragma once

#include <memory>
#include <variant>

#include "ir.hpp"

namespace funnyir {

using Array = std::shared_ptr<std::vector<std::int32_t>>;
using Value = std::variant<std::int32_t, Array>;

std::vector<Value> execute(const ModuleIR& module, const std::string& function,
                           const std::vector<Value>& arguments, size_t stepLimit = 100000);

}  // namespace funnyir
