#pragma once
#include <memory>
#include <string>
#include <vector>

namespace funnyparse {

// Виды узлов AST. Раскладка детей (kids) для каждого вида — в docs/ast.md.
enum class Kind {
    // верхний уровень
    Program, Function, Formula, Params, Returns, Locals, VarDef,
    Requires, Ensures, Invariant,
    // операторы
    Block, Assign, ArrayAssign, TupleAssign, Targets, If, While, Assert, Assume,
    // арифметические выражения
    Number, Var, Neg, Binary, Call, Index,
    // условия и предикаты
    BoolConst, Compare, Not, And, Or, Implies, Forall, Exists, FormulaRef
};

struct Node;
using NodePtr = std::unique_ptr<Node>;

// Один универсальный узел: вид + две строки (text/extra) + позиция + дети.
//   text  — имя / оператор / число / true|false (зависит от вида)
//   extra — тип переменной ("int" или "int[]") у VarDef, Forall, Exists
//   line, col — позиция (с 1) первого значимого токена узла
struct Node {
    Kind kind;
    std::string text, extra;
    int line = 0, col = 0;
    std::vector<NodePtr> kids;

    Node(Kind k, std::string t, int l, int c) : kind(k), text(std::move(t)), line(l), col(c) {}
    // Первый прямой ребёнок заданного вида или nullptr (удобно для необязательных Requires/Ensures/...).
    const Node* find(Kind k) const;
};

// Имя вида узла: "Program", "Binary", ...
const char* kindName(Kind k);

// Дерево с отступами, по строке на узел (стабильно, годится для snapshot-тестов).
std::string dumpTree(const Node& n);
// То же дерево в JSON (с line/col).
std::string dumpJSON(const Node& n);
// Компактная S-expression в одну строку: "(+ 1 (* 2 3))" — удобно для тестов.
std::string dumpSexpr(const Node& n);

}