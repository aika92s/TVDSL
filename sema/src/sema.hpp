#pragma once
#include <map>
#include <string>
#include <vector>

#include "ast.hpp"
#include "parser.hpp"

namespace funnysema {

// Типы Funny, которые знает проверка.
//   Int      — int (32 бита со знаком)
//   IntArray — int[]
//   Bool     — тип условий и предикатов (в переменных не встречается)
//   Tuple    — результат вызова функции с несколькими returns (только справа в кортежном присваивании)
//   Error    — "тип неизвестен из-за уже найденной ошибки": совместим с любым типом, каскадных ошибок нет
//   None     — у узла нет типа (Program, Function, Block, If, ...)
enum class Ty { None, Int, IntArray, Bool, Tuple, Error };

const char* tyName(Ty t);  // "int", "int[]", "bool", "error"; для None и Tuple — ""

// Что известно о узле после проверки (аннотация).
struct NodeInfo {
    Ty type = Ty::None;
    std::vector<Ty> tuple;   // состав кортежа, если type == Tuple
    std::string symbol;      // Var / Assign / ArrayAssign / Index / VarDef: роль переменной
                             //   "param" | "return" | "local" | "quantifier";
                             // Call / FormulaRef: что вызывается "function" | "builtin" | "formula"
    std::string signature;   // только у Function / Formula, например "(int,int[])->(int)"
};

// Сигнатура функции или формулы (таблица определений модуля).
struct Signature {
    std::string name;
    bool isFormula = false;
    std::vector<Ty> params;
    std::vector<Ty> returns;  // у формулы пусто
    int line = 0, col = 0;    // где определена
};

struct SemaResult {
    std::map<const funnyparse::Node*, NodeInfo> info;  // аннотации; ключ — узел AST (AST должен жить дольше)
    std::map<std::string, Signature> functions;        // таблица функций и формул модуля
    std::vector<funnyparse::Diagnostic> errors;        // отсортированы по позиции
    bool ok() const { return errors.empty(); }
    // Аннотация узла (пустая, если узла нет в таблице).
    const NodeInfo& at(const funnyparse::Node& n) const;
};

// Строка типа узла: "int", "int[]", "bool", "error", кортеж "(int,int[])"; пустая, если типа нет.
std::string typeString(const NodeInfo& i);

// Проверка имён и типов программы (корень — узел Program, полученный без синтаксических ошибок).
// Не останавливается на первой ошибке.
SemaResult analyze(const funnyparse::Node& program);

// Аннотированный AST: дерево с отступами / JSON / S-expression (форматы — в docs/annotated_ast.md).
std::string dumpAnnotatedTree(const funnyparse::Node& n, const SemaResult& r);
std::string dumpAnnotatedJSON(const funnyparse::Node& n, const SemaResult& r);
std::string dumpAnnotatedSexpr(const funnyparse::Node& n, const SemaResult& r);

}  // namespace funnysema
