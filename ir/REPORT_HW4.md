# Отчёт HW4

Промежуточное представление Funny строится из аннотированного AST HW3. Реализованы трёхадресный IR, базовые блоки и CFG, локальные оптимизации, анализ живости, граф конфликтов и жадная раскраска временных. Формат IR и API описаны в `docs/ir.md`, команды запуска — в `README.md`.

## Результат: 48/48 сценариев пройдено

| группа | пройдено | всего |
|---|---:|---:|
| cfg | 5 | 5 |
| coloring | 2 | 2 |
| equivalence | 7 | 7 |
| flags | 4 | 4 |
| format | 3 | 3 |
| generation | 11 | 11 |
| liveness | 2 | 2 |
| metrics | 1 | 1 |
| optimization | 11 | 11 |
| pipeline | 2 | 2 |

## Покрытие требований

- генерация: арифметика, логика, ветвления, циклы, массивы, `length`, обычные и многорезультатные вызовы, явный `return`, таблица типов;
- CFG: лидеры, базовые блоки, прямые/обратные рёбра, согласованность predecessors/successors;
- оптимизации: константы, заявленные алгебраические правила, DCE и сохранение исключений; каждый проход отключается отдельно;
- корректность: выполнение до/после сравнивается на 87 наборах скалярных входов, вызовах, кортежах и массивах;
- живость/раскраска: конфликтные рёбра, неподвижная точка на цикле, корректность цветов и метрики;
- форматы: текст, JSON, CFG DOT, interference DOT; ошибки front-end не допускаются до генератора.

## Подробные тесты

| имя | группа | вход / условие | ожидание | получено | результат |
|---|---|---|---|---|---|
| arithmetic_precedence | generation | `r = 1 + 2 * x;` | `mul before add` | `mul before add` | PASS |
| unary_and_comparison | generation | `if (-x <= 0) ...` | `neg=yes, le=yes, branch=yes` | `neg=yes, le=yes, branch=yes` | PASS |
| if_else_labels | generation | `if (x > 0) r = 1; else r = 2;` | `branches=1, jumps=2` | `branches=1, jumps=2` | PASS |
| while_loop | generation | `while (i < n) i = i + 1;` | `branch=yes, back-jump=yes` | `branch=yes, back-jump=yes` | PASS |
| single_result_call | generation | `r = inc(x);` | `call inc -> one destination` | `call inc -> one destination` | PASS |
| tuple_call | generation | `a, b = pair(x);` | `call pair -> 2 destinations` | `call pair -> 2 destinations` | PASS |
| array_load_store | generation | `b[i] = a[i] + 1;` | `load=yes, store=yes` | `load=yes, store=yes` | PASS |
| builtin_length | generation | `r = length(a);` | `call length` | `call length` | PASS |
| explicit_return | generation | `f returns r, s` | `return operands=2` | `return operands=2` | PASS |
| ir_type_table | generation | `int and int[] values` | `x=int, a=int[], temporary=int` | `x=int, a=int[], temporary=int` | PASS |
| verification_contracts_not_executable | generation | `requires/ensures/invariant` | `IR generated` | `IR generated` | PASS |
| straight_line | cfg | `three assignments` | `blocks=1` | `blocks=1` | PASS |
| if_two_successors | cfg | `if/else` | `two-way block=yes` | `two-way block=yes` | PASS |
| while_back_edge | cfg | `while` | `back edge=yes` | `back edge=yes` | PASS |
| predecessor_consistency | cfg | `nested if in while` | `all reverse edges=yes` | `all reverse edges=yes` | PASS |
| basic_block_terminators | cfg | `if plus while` | `terminators valid=yes` | `terminators valid=yes` | PASS |
| constant_folding | optimization | `r = 1 + 2 * 3;` | `r = copy 7` | `r = copy 7` | PASS |
| constant_propagation | optimization | `a = 4; r = a + 2;` | `r = copy 6` | `r = copy 6` | PASS |
| algebra_add_zero | optimization | `r = x + 0;` | `add removed=yes` | `add removed=yes` | PASS |
| algebra_sub_zero | optimization | `r = x - 0;` | `sub removed=yes` | `sub removed=yes` | PASS |
| algebra_mul_one | optimization | `r = x * 1;` | `mul removed=yes` | `mul removed=yes` | PASS |
| algebra_mul_zero | optimization | `r = x * 0;` | `r = copy 0` | `r = copy 0` | PASS |
| algebra_div_one | optimization | `r = x / 1;` | `div removed=yes` | `div removed=yes` | PASS |
| boolean_identities | optimization | `true and (x > 0) or false` | `and/or removed=yes` | `and/or removed=yes` | PASS |
| local_dce | optimization | `unused pure temporary` | `dead add removed=yes` | `dead add removed=yes` | PASS |
| dce_keeps_division_trap | optimization | `unused 1 / 0` | `division kept=yes` | `division kept=yes` | PASS |
| dce_keeps_bounds_check | optimization | `unused a[i]` | `load kept=yes` | `load kept=yes` | PASS |
| disable_constant_propagation | flags | `--no-const-prop` | `add remains=yes` | `add remains=yes` | PASS |
| disable_algebra | flags | `--no-algebra` | `add remains=yes` | `add remains=yes` | PASS |
| disable_dce | flags | `--no-dce` | `dead temporary remains=yes` | `dead temporary remains=yes` | PASS |
| disable_all | flags | `--no-opt` | `IR unchanged=yes` | `IR unchanged=yes` | PASS |
| calculation_if_while | equivalence | `x = 0..20` | `21/21 equal` | `21/21 equal` | PASS |
| three_way_branch | equivalence | `x = -20..20` | `41/41 equal` | `41/41 equal` | PASS |
| nested_calls | equivalence | `square(inc(x)) for x=-10..10` | `21/21 equal` | `21/21 equal` | PASS |
| tuple_results | equivalence | `q, r = pair(x)` | `result=(7,8) both` | `result=(7,8) both` | PASS |
| array_sum | equivalence | `[2,4,6,8]` | `sum=20 both` | `sum=20 both` | PASS |
| array_store | equivalence | `b[0] = b[0] + 1` | `first=11 both` | `first=11 both` | PASS |
| int32_wraparound | equivalence | `2147483647 + 1` | `result=-2147483648 both` | `result=-2147483648 both` | PASS |
| interference_edge | liveness | `(x+y) * (x-y)` | `edges>=1` | `edges>=1` | PASS |
| fixed_point_across_loop | liveness | `temporary condition in while` | `loop live sets computed=yes` | `loop live sets computed=yes` | PASS |
| adjacent_colors_differ | coloring | `conflict graph` | `valid=yes` | `valid=yes` | PASS |
| color_count | coloring | `one conflict edge` | `colors=2` | `colors=2` | PASS |
| before_after | metrics | `constant plus +0` | `instructions reduced=yes, temporaries reduced=yes` | `instructions reduced=yes, temporaries reduced=yes` | PASS |
| json_contains_types | format | `JSON IR` | `functions/op/types=yes` | `functions/op/types=yes` | PASS |
| cfg_dot | format | `DOT CFG` | `digraph and edges=yes` | `digraph and edges=yes` | PASS |
| interference_dot | format | `DOT interference` | `graph/colors/edge=yes` | `graph/colors/edge=yes` | PASS |
| syntax_error_stops_ir | pipeline | `f() returns r: int r = ;` | `syntax errors=1, IR not built` | `syntax errors=1, IR not built` | PASS |
| semantic_error_stops_ir | pipeline | `r = unknown;` | `semantic errors=1, generator rejects=yes` | `semantic errors=1, generator rejects=yes` | PASS |

## Эффект оптимизаций на примере

Для `examples/optimization.fn`: 30 → 28 инструкций, 10 → 8 временных, 7 базовых блоков, 2 цвета. Фактический вывод находится в `examples/output.txt`, графы — в `examples/cfg.dot` и `examples/interference.dot`.

`div` и `load` не удаляются как мёртвые: они сохраняют деление на ноль и проверку границ массива. Формулы, `requires`, `ensures` и инварианты остаются данными статической верификации и не попадают в исполняемое тело.
