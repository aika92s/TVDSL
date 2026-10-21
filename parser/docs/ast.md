# Формат AST

API: `src/ast.hpp`. Узел один для всех видов: `Node { Kind kind; string text, extra; int line, col; vector<NodePtr> kids; }`.
`line`/`col` считаются с 1, столбец в байтах; это позиция первого значимого токена узла
(у бинарных операций — позиция самого оператора, у вызова — имени функции).

Вывод: `dumpTree` (отступы, годится для snapshot), `dumpJSON` (с `line`/`col`), `dumpSexpr` (одна строка, для тестов).
Поиск необязательных частей: `node.find(Kind::Requires)` — первый ребёнок такого вида или `nullptr`.

| Kind | text | extra | kids |
|---|---|---|---|
| Program | | | Function \| Formula … |
| Function | имя | | Params, [Requires], Returns, [Ensures], [Locals], тело (оператор) |
| Formula | имя | | Params, предикат |
| Params / Returns / Locals | | | VarDef … |
| VarDef | имя | `int` или `int[]` | |
| Requires / Ensures / Invariant | | | предикат |
| Block | | | оператор … |
| Assign | имя переменной | | выражение |
| ArrayAssign | имя массива | | индекс, значение |
| TupleAssign | | | Targets, Call |
| Targets | | | Var … |
| If | | | условие, then, [else] |
| While | | | условие, [Invariant], тело |
| Assert / Assume | | | предикат |
| Number | число (текстом) | | |
| Var | имя | | |
| Neg | | | операнд |
| Binary | `+ - * /` | | левый, правый |
| Call | имя (`length` — встроенная) | | аргументы … |
| Index | имя массива | | индекс |
| BoolConst | `true`/`false` | | |
| Compare | `== != <= >= < >` | | левый, правый |
| Not | | | операнд |
| And / Or / Implies | | | левый, правый |
| Forall / Exists | имя переменной | тип | предикат |
| FormulaRef | имя | | аргументы … |

Порядок детей Function совпадает с порядком в исходнике; необязательные Requires, Ensures, Locals присутствуют
только если были написаны. Тело всегда последний ребёнок. У While тело — последний ребёнок, условие — первый.

Пример (`x = a[i] + 1;`):

```
Assign x
  Binary +
    Index a
      Var i
    Number 1
```
