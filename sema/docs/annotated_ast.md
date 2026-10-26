# Формат аннотированного AST

| Узел | `type` | `symbol` | `signature` |
|---|---|---|---|
| Number, Neg, Binary, Index | `int` | у Index — роль массива | |
| Var | тип переменной | роль: `param` / `return` / `local` / `quantifier` | |
| Call | тип результата; `(t1,t2)` для кортежа в `TupleAssign` | `function` / `builtin` (`length`) | |
| BoolConst, Compare, Not, And, Or, Implies, Forall, Exists, FormulaRef | `bool` | у FormulaRef — `formula` | |
| VarDef | объявленный тип | роль: `param` / `return` / `local` | |
| Assign, ArrayAssign | тип переменной-цели | роль цели | |
| Function / Formula | — | `function` / `formula` | `(int,int[])->(int)` / `(int)->bool` |
| остальные (Program, Block, If, While, Requires, ...) | — | | |

У Forall/Exists в JSON есть ещё `var_type` — тип переменной квантора.

## Три вида вывода (`funny_check`)

Дерево (`Kind текст -> тип`):

```
Program
  Function inc  (int)->(int)
    Params
      VarDef x: int
    Returns
      VarDef r: int
    Assign r -> int
      Binary + -> int
        Var x -> int
        Number 1 -> int
```

JSON (`--json`; поля узла: `kind`, `text`, `line`, `col`, `type`, `symbol`, `signature`, `var_type`, `children` — пустые опускаются):

```json
{"kind": "Binary", "text": "+", "line": 1, "col": 34, "type": "int", "children": [
  {"kind": "Var", "text": "x", "line": 1, "col": 32, "type": "int", "symbol": "param"},
  {"kind": "Number", "text": "1", "line": 1, "col": 36, "type": "int"}
]}
```

S-expression (`--sexpr`): после имени вида (у `Binary`/`Compare` — после оператора) через двоеточие тип; у `Number`/`Var` после текста:
`(+:int x:int 1:int)`, `(Call:(int,int) divide n:int 3:int)`, `(Assign:int[] b a:int[])`.

Ошибки печатаются отдельно (stderr), AST выводится и при семантических ошибках — узлы с ошибкой имеют тип `error`.
