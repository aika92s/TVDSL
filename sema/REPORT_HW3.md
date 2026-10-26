# Отчёт HW3

Семантический анализ Funny: таблицы символов с областями видимости, проверка типов выражений, операторов и вызовов, диагностика с позициями, аннотированный AST. Вход — AST парсера HW2 (под ним лексер HW1); типы и ограничения описаны в `docs/types.md`, формат аннотированного AST — в `docs/annotated_ast.md`.

## Тесты: 169/169 пройдено (good 37, typed 18, bad 99, annot 11, robust 4)

`good` — программа корректна, ошибок нет; `typed` — аннотированный AST (S-выражение, `узел:тип`) должен совпасть с эталоном; `bad` — должны выдаваться ровно ожидаемые диагностики (`строка:столбец: сообщение`; `syntax:` — ошибка парсера); `annot` — формат аннотированного AST (JSON, дерево, таблица функций) и диагностик; `robust` — проверка не должна падать на испорченных программах.

| имя | вид | вход | ожидание | результат |
|---|---|---|---|---|
| unknown_variable_rhs | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r = zz;\n}\n` | `3:7: unknown variable 'zz'` | PASS |
| unknown_variable_lhs | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  zz = 1;\n}\n` | `3:3: unknown variable 'zz'` | PASS |
| unknown_array_in_index | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r = zz[0];\n}\n` | `3:7: unknown variable 'zz'` | PASS |
| unknown_array_in_store | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  zz[0] = 1;\n}\n` | `3:3: unknown variable 'zz'` | PASS |
| unknown_in_condition | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  if (zz > 0) r = 1;\n}\n` | `3:7: unknown variable 'zz'` | PASS |
| unknown_in_assert | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  assert zz > 0;\n}\n` | `3:10: unknown variable 'zz'` | PASS |
| unknown_function | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r = g(1);\n}\n` | `3:7: unknown function 'g'` | PASS |
| unknown_function_and_variable | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r = g(zz);\n}\n` | `3:7: unknown function 'g'`<br>`3:9: unknown variable 'zz'` | PASS |
| unknown_formula | bad | `f() returns r: int ensures zz(1) r = 1;` | `1:28: unknown formula 'zz'` | PASS |
| unknown_variable_in_formula_body | bad | `p(x: int) => y > 0` | `1:14: unknown variable 'y'` | PASS |
| variable_names_are_case_sensitive | bad | `f(x: int) returns r: int r = X;` | `1:30: unknown variable 'X'` | PASS |
| duplicate_parameter | bad | `f(x: int, x: int) returns r: int r = x;` | `1:11: duplicate declaration of 'x': already declared as parameter at 1:3` | PASS |
| duplicate_param_and_return | bad | `f(x: int) returns x: int r = 1;` | `1:19: duplicate declaration of 'x': already declared as parameter at 1:3`<br>`1:26: unknown variable 'r'` | PASS |
| duplicate_return_and_local | bad | `f() returns r: int uses r: int r = 1;` | `1:25: duplicate declaration of 'r': already declared as return value at 1:13` | PASS |
| duplicate_return_return | bad | `f() returns r: int, r: int[] r = 1;` | `1:21: duplicate declaration of 'r': already declared as return value at 1:13` | PASS |
| duplicate_local_local | bad | `f() returns r: int uses i: int, i: int r = 1;` | `1:33: duplicate declaration of 'i': already declared as local variable at 1:25` | PASS |
| duplicate_param_and_local | bad | `f(i: int) returns r: int uses i: int r = i;` | `1:31: duplicate declaration of 'i': already declared as parameter at 1:3` | PASS |
| duplicate_formula_parameter | bad | `p(x: int, x: int[]) => x > 0` | `1:11: duplicate declaration of 'x': already declared as parameter at 1:3` | PASS |
| duplicate_function | bad | `f() returns r: int r = 1;\nf() returns r: int r = 2;` | `2:1: redefinition of 'f': already defined as function at 1:1 (names must be unique in a module, no overloads)` | PASS |
| no_overloading_by_parameters | bad | `f(x: int) returns r: int r = x;\nf(x: int[]) returns r: int r = 1;` | `2:1: redefinition of 'f': already defined as function at 1:1 (names must be unique in a module, no overloads)` | PASS |
| function_and_formula_same_name | bad | `p(x: int) => x > 0\np() returns r: int r = 1;` | `2:1: redefinition of 'p': already defined as formula at 1:1 (names must be unique in a module, no overloads)` | PASS |
| duplicate_formula | bad | `p(x: int) => x > 0\np(x: int) => x > 1` | `2:1: redefinition of 'p': already defined as formula at 1:1 (names must be unique in a module, no overloads)` | PASS |
| body_of_duplicate_is_still_checked | bad | `f() returns r: int r = 1;\nf() returns r: int r = zz;` | `2:1: redefinition of 'f': already defined as function at 1:1 (names must be unique in a module, no overloads)`<br>`2:24: unknown variable 'zz'` | PASS |
| same_names_in_different_functions | good | `f(x: int) returns r: int r = x;\ng(x: int) returns r: int r = x;` | `(no errors)` | PASS |
| variable_named_like_a_function | good | `g() returns r: int r = 1;\nf() returns g: int g = g();` | `(no errors)` | PASS |
| parameter_named_like_a_formula | good | `p(x: int) => x > 0\nf(p: int) returns r: int ensures p > 0 r = p;` | `(no errors)` | PASS |
| call_of_function_defined_below | good | `f() returns r: int r = g(1);\ng(x: int) returns q: int q = x;` | `(no errors)` | PASS |
| recursion | good | `fact(n: int) returns r: int uses t: int { if (n <= 1) r = 1; else { t = fact(n - 1); r = n * t; } }` | `(no errors)` | PASS |
| mutual_recursion | good | `even(n: int) returns r: int { if (n == 0) r = 1; else r = odd(n - 1); }\nodd(n: int) returns r: int { if (n == 0) r = 0; else r = even(n - 1); }` | `(no errors)` | PASS |
| formula_used_before_definition | good | `f(x: int) returns r: int ensures big(x) r = x;\nbig(x: int) => x > 100` | `(no errors)` | PASS |
| requires_sees_only_parameters | bad | `f(x: int) requires r > 0 returns r: int r = x;` | `1:20: variable 'r' (return value declared at 1:34) is not visible in the precondition (requires), where only parameters are visible` | PASS |
| requires_does_not_see_local | bad | `f(x: int) requires i > 0 returns r: int uses i: int r = x;` | `1:20: variable 'i' (local variable declared at 1:46) is not visible in the precondition (requires), where only parameters are visible` | PASS |
| ensures_does_not_see_local | bad | `f(x: int) returns r: int ensures i > 0 uses i: int r = x;` | `1:34: variable 'i' (local variable declared at 1:45) is not visible in the postcondition (ensures), where only parameters and return values are visible` | PASS |
| formula_body_sees_only_its_parameters | bad | `f(x: int) returns r: int r = x;\np(y: int) => x > 0` | `2:14: unknown variable 'x'` | PASS |
| ensures_sees_parameters_and_returns | good | `f(x: int) returns r: int ensures r == x r = x;` | `(no errors)` | PASS |
| invariant_sees_everything | good | `f(x: int) returns r: int uses i: int\n{ i = 0; r = 0; while (i < x) invariant 0 <= i and i <= x and r == i { r = r + 1; i = i + 1; } }` | `(no errors)` | PASS |
| assert_and_assume_see_everything | good | `f(x: int) returns r: int uses i: int { i = x; assume i > 0; r = i; assert r == x and i == x; }` | `(no errors)` | PASS |
| invariant_unknown_variable | bad | `f(x: int) returns r: int while (x > 0) invariant zz > 0 r = 1;` | `1:50: unknown variable 'zz'` | PASS |
| quantifier_duplicates_parameter_in_ensures | bad | `f(x: int) returns r: int ensures forall (x: int \| x > 0) r = 1;` | `1:34: quantifier variable 'x' duplicates the parameter 'x' declared at 1:3 (names in a scope must be unique)` | PASS |
| quantifier_duplicates_return_in_ensures | bad | `f(x: int) returns r: int ensures exists (r: int \| r > x) r = 1;` | `1:34: quantifier variable 'r' duplicates the return value 'r' declared at 1:19 (names in a scope must be unique)` | PASS |
| quantifier_duplicates_local_in_invariant | bad | `f(x: int) returns r: int uses i: int while (x > 0) invariant forall (i: int \| i > 0) r = 1;` | `1:62: quantifier variable 'i' duplicates the local variable 'i' declared at 1:31 (names in a scope must be unique)` | PASS |
| quantifier_duplicates_enclosing_quantifier | bad | `f(x: int) returns r: int ensures forall (i: int \| exists (i: int \| i > 0)) r = 1;` | `1:51: quantifier variable 'i' is already declared by an enclosing quantifier at 1:34` | PASS |
| quantifier_duplicates_formula_parameter | bad | `p(x: int) => forall (x: int \| x > 0)` | `1:14: quantifier variable 'x' duplicates the parameter 'x' declared at 1:3 (names in a scope must be unique)` | PASS |
| quantifier_may_reuse_return_name_in_requires | good | `f(x: int) requires forall (r: int \| r > x) returns r: int r = x;` | `(no errors)` | PASS |
| quantifier_may_reuse_local_name_in_ensures | good | `f(x: int) returns r: int ensures forall (i: int \| i > 0 or x > 0) uses i: int r = x;` | `(no errors)` | PASS |
| sibling_quantifiers_may_share_a_name | good | `f(x: int) returns r: int ensures forall (i: int \| i > 0 or x > 0) and exists (i: int \| i > x) r = x;` | `(no errors)` | PASS |
| quantifier_variable_not_visible_after_it | bad | `f(x: int) returns r: int ensures (forall (i: int \| i > 0)) and i > 0 r = 1;` | `1:64: unknown variable 'i'` | PASS |
| assign_array_to_int | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r = b;\n}\n` | `3:7: type mismatch in assignment to 'r': expected int, found int[]` | PASS |
| assign_int_to_array | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  s = a;\n}\n` | `3:7: type mismatch in assignment to 's': expected int[], found int` | PASS |
| assign_array_to_local_int | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  i = s;\n}\n` | `3:7: type mismatch in assignment to 'i': expected int, found int[]` | PASS |
| operator_error_is_reported_at_the_operator | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r = b + 1;\n  r = 1 + 2 * b;\n}\n` | `3:9: operator '+' requires int operands, found int[] and int`<br>`4:13: operator '*' requires int operands, found int and int[]` | PASS |
| assign_int_and_array | good | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r = a + 1;\n  s = b;\n  i = r;\n}\n` | `(no errors)` | PASS |
| local_array_not_allowed | bad | `f() returns r: int uses a: int[] r = 1;` | `1:25: local variable 'a' cannot have type int[] (local variables can only be int)` | PASS |
| return_array_allowed | good | `f(a: int[]) returns b: int[] b = a;` | `(no errors)` | PASS |
| add_array | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r = b + 1;\n}\n` | `3:9: operator '+' requires int operands, found int[] and int` | PASS |
| mul_two_arrays | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r = b * b;\n}\n` | `3:9: operator '*' requires int operands, found int[] and int[]` | PASS |
| sub_array_right | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r = a - s;\n}\n` | `3:9: operator '-' requires int operands, found int and int[]` | PASS |
| div_array | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r = b / 2;\n}\n` | `3:9: operator '/' requires int operands, found int[] and int` | PASS |
| neg_array | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r = -b;\n}\n` | `3:7: unary '-' requires an int operand, found int[]` | PASS |
| neg_array_in_expression | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r = 1 + -b;\n}\n` | `3:11: unary '-' requires an int operand, found int[]` | PASS |
| compare_arrays | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  if (b == b) r = 1;\n}\n` | `3:9: operator '==' requires int operands, found int[] and int[]` | PASS |
| compare_array_and_int | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  while (s < 1) r = 1;\n}\n` | `3:12: operator '<' requires int operands, found int[] and int` | PASS |
| compare_in_assert | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  assert b != a;\n}\n` | `3:12: operator '!=' requires int operands, found int[] and int` | PASS |
| index_of_int | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r = a[0];\n}\n` | `3:7: 'a' is not an array (its type is int), it cannot be indexed` | PASS |
| index_is_array | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r = b[b];\n}\n` | `3:9: array index must be int, found int[]` | PASS |
| index_expression_is_checked | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r = b[a + b];\n}\n` | `3:11: operator '+' requires int operands, found int and int[]` | PASS |
| store_into_int | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r[0] = 1;\n}\n` | `3:3: 'r' is not an array (its type is int), it cannot be indexed` | PASS |
| store_index_is_array | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  s[b] = 1;\n}\n` | `3:5: array index must be int, found int[]` | PASS |
| store_value_is_array | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  s[0] = s;\n}\n` | `3:10: array element must be int, found int[]` | PASS |
| store_and_load | good | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  s = b;\n  s[a] = b[0] + 1;\n  r = s[a];\n  i = length(s) - 1;\n}\n` | `(no errors)` | PASS |
| assign_to_parameter | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  a = 1;\n}\n` | `3:3: cannot assign to parameter 'a' (parameters are read-only)` | PASS |
| assign_to_array_parameter | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  b = s;\n}\n` | `3:3: cannot assign to parameter 'b' (parameters are read-only)` | PASS |
| store_into_array_parameter | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  b[0] = 1;\n}\n` | `3:3: cannot modify array parameter 'b' (parameters, including arrays, are read-only)` | PASS |
| parameter_assign_still_checks_rhs_type | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  a = b;\n}\n` | `3:3: cannot assign to parameter 'a' (parameters are read-only)`<br>`3:7: type mismatch in assignment to 'a': expected int, found int[]` | PASS |
| literal_too_big | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r = 2147483648;\n}\n` | `3:7: integer literal 2147483648 is out of range for int (32-bit signed: 0..2147483647)` | PASS |
| literal_way_too_big | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r = 99999999999;\n}\n` | `3:7: integer literal 99999999999 is out of range for int (32-bit signed: 0..2147483647)` | PASS |
| negative_literal_too_small | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r = -2147483649;\n}\n` | `3:8: integer literal 2147483649 is out of range for int (32-bit signed: -2147483648..2147483647)` | PASS |
| literal_int_max_and_min | good | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r = 2147483647;\n  r = -2147483648;\n  r = 0;\n}\n` | `(no errors)` | PASS |
| length_of_int | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r = length(a);\n}\n` | `3:14: argument 1 of built-in 'length': expected int[], found int` | PASS |
| length_no_args | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r = length();\n}\n` | `3:7: built-in 'length' expects 1 argument(s), found 0` | PASS |
| length_two_args | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r = length(b, b);\n}\n` | `3:7: built-in 'length' expects 1 argument(s), found 2` | PASS |
| length_of_array | good | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r = length(b) + length(s);\n}\n` | `(no errors)` | PASS |
| call_too_few_arguments | bad | `g(x: int, c: int[]) returns q: int q = x;\nf() returns r: int r = g(1);` | `2:24: function 'g' expects 2 argument(s), found 1` | PASS |
| call_too_many_arguments | bad | `g(x: int, c: int[]) returns q: int q = x;\nf(b: int[]) returns r: int r = g(1, b, 3);` | `2:32: function 'g' expects 2 argument(s), found 3` | PASS |
| call_no_arguments | bad | `g(x: int, c: int[]) returns q: int q = x;\nf() returns r: int r = g();` | `2:24: function 'g' expects 2 argument(s), found 0` | PASS |
| call_argument_types_swapped | bad | `g(x: int, c: int[]) returns q: int q = x;\nf(b: int[]) returns r: int r = g(b, 1);` | `2:34: argument 1 of function 'g': expected int, found int[]`<br>`2:37: argument 2 of function 'g': expected int[], found int` | PASS |
| call_argument_expression_position | bad | `g(x: int, c: int[]) returns q: int q = x;\nf(b: int[]) returns r: int r = g(1 + 2, b + 1);` | `2:43: operator '+' requires int operands, found int[] and int` | PASS |
| call_argument_error_inside_argument | bad | `g(x: int, c: int[]) returns q: int q = x;\nf() returns r: int r = g(zz, zz);` | `2:26: unknown variable 'zz'`<br>`2:30: unknown variable 'zz'` | PASS |
| call_with_correct_arguments | good | `g(x: int, c: int[]) returns q: int q = x;\nf(b: int[]) returns r: int r = g(1 + 2, b) * g(r, b);` | `(no errors)` | PASS |
| call_returns_array | good | `h(a: int[]) returns b: int[] b = a;\nf(a: int[]) returns r: int uses i: int { i = length(h(a)); r = i; }` | `(no errors)` | PASS |
| array_result_assigned | good | `h(a: int[]) returns b: int[] b = a;\nf(a: int[]) returns s: int[] s = h(a);` | `(no errors)` | PASS |
| array_result_assigned_to_int | bad | `h(a: int[]) returns b: int[] b = a;\nf(a: int[]) returns r: int r = h(a);` | `2:32: type mismatch in assignment to 'r': expected int, found int[]` | PASS |
| multi_return_in_expression | bad | `d() returns q: int, r: int q = 1;\nf() returns r: int r = d() + 1;` | `2:24: function 'd' returns 2 values and cannot be used in an expression (use a tuple assignment: a, b = d(...);)` | PASS |
| multi_return_single_assignment | bad | `d() returns q: int, r: int q = 1;\nf() returns r: int r = d();` | `2:24: function 'd' returns 2 values and cannot be used in an expression (use a tuple assignment: a, b = d(...);)` | PASS |
| multi_return_as_argument | bad | `d() returns q: int, r: int q = 1;\ng(x: int) returns r: int r = x;\nf() returns r: int r = g(d());` | `3:26: function 'd' returns 2 values and cannot be used in an expression (use a tuple assignment: a, b = d(...);)` | PASS |
| tuple_too_many_targets | bad | `d() returns q: int, r: int q = 1;\nf() returns a: int, b: int, c: int\n{\n  a, b, c = d();\n}\n` | `4:3: tuple assignment has 3 target(s), but 'd' returns 2 value(s)` | PASS |
| tuple_from_single_return_function | bad | `g(x: int) returns q: int q = x;\nf() returns a: int, b: int\n{\n  a, b = g(1);\n}\n` | `4:3: tuple assignment has 2 target(s), but 'g' returns 1 value(s)` | PASS |
| tuple_target_type_mismatch | bad | `d() returns q: int, r: int[] q = 1;\nf() returns a: int, b: int\n{\n  a, b = d();\n}\n` | `4:6: cannot assign value #2 of 'd' (type int[]) to 'b' of type int` | PASS |
| tuple_assign_to_parameter | bad | `d() returns q: int, r: int q = 1;\nf(x: int) returns a: int\n{\n  x, a = d();\n}\n` | `4:3: cannot assign to parameter 'x' (parameters are read-only)` | PASS |
| tuple_unknown_target_and_function | bad | `f() returns a: int\n{\n  u, a = zz();\n}\n` | `3:3: unknown variable 'u'`<br>`3:10: unknown function 'zz'` | PASS |
| tuple_argument_type | bad | `d(x: int[]) returns q: int, r: int q = 1;\nf() returns a: int, b: int\n{\n  a, b = d(1);\n}\n` | `4:12: argument 1 of function 'd': expected int[], found int` | PASS |
| tuple_assignment | good | `d() returns q: int, r: int q = 1;\nf() returns a: int, b: int uses t: int { a, b = d(); t, a = d(); }` | `(no errors)` | PASS |
| tuple_with_array_result | good | `d() returns q: int, r: int[] q = 1;\nf() returns a: int, b: int[] { a, b = d(); }` | `(no errors)` | PASS |
| formula_in_expression | bad | `p(x: int) => x > 0\nf() returns r: int r = p(1);` | `2:24: 'p' is a formula: formulas can be used only in predicates (requires, ensures, invariant, assert, assume, formula bodies)` | PASS |
| formula_in_condition | bad | `p(x: int) => x > 0\nf() returns r: int if (p(1) > 0) r = 1;` | `2:24: 'p' is a formula: formulas can be used only in predicates (requires, ensures, invariant, assert, assume, formula bodies)` | PASS |
| function_as_predicate | bad | `g(x: int) returns q: int q = x;\nf() returns r: int ensures g(1) r = 1;` | `2:28: 'g' is a function, not a formula: a bare function call is not a predicate (compare its result, e.g. g(...) > 0)` | PASS |
| formula_wrong_argument_count | bad | `p(x: int) => x > 0\nf() returns r: int ensures p(1, 2) r = 1;` | `2:28: formula 'p' expects 1 argument(s), found 2` | PASS |
| formula_wrong_argument_type | bad | `p(x: int) => x > 0\nf(b: int[]) returns r: int ensures p(b) r = 1;` | `2:38: argument 1 of formula 'p': expected int, found int[]` | PASS |
| formula_in_predicates | good | `p(x: int, v: int[]) => x >= 0 and x < length(v)\nf(a: int, b: int[]) requires p(a, b) returns r: int ensures p(r, b) or not p(a, b) { assert p(a, b); r = b[a]; }` | `(no errors)` | PASS |
| function_in_predicate_comparison | good | `g(x: int) returns q: int q = x;\nf(a: int) returns r: int ensures g(a) > 0 and g(r) == a r = a;` | `(no errors)` | PASS |
| formula_calls_formula_and_function | good | `g(x: int) returns q: int q = x;\np(x: int) => x > 0\nq(x: int) => p(x) and g(x) > 1` | `(no errors)` | PASS |
| several_errors_in_one_function | bad | `f(x: int, x: int[]) returns r: int ensures true uses r: int\n{\n  y = 1;\n  x = 2;\n  r = x + 1;\n  r = g(1);\n  r = f(1);\n  r = length(r);\n}\n` | `1:11: duplicate declaration of 'x': already declared as parameter at 1:3`<br>`1:54: duplicate declaration of 'r': already declared as return value at 1:29`<br>`3:3: unknown variable 'y'`<br>`4:3: cannot assign to parameter 'x' (parameters are read-only)`<br>`6:7: unknown function 'g'`<br>`7:7: function 'f' expects 2 argument(s), found 1`<br>`8:14: argument 1 of built-in 'length': expected int[], found int` | PASS |
| errors_in_different_functions | bad | `f() returns r: int r = zz;\ng() returns r: int r = ww;\nh() returns r: int r = vv;\n` | `1:24: unknown variable 'zz'`<br>`2:24: unknown variable 'ww'`<br>`3:24: unknown variable 'vv'` | PASS |
| errors_in_nested_statements | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  if (zz > 0) { r = b; } else { s = a; }\n  while (ww > 0) { a = 1; }\n}\n` | `3:7: unknown variable 'zz'`<br>`3:21: type mismatch in assignment to 'r': expected int, found int[]`<br>`3:37: type mismatch in assignment to 's': expected int[], found int`<br>`4:10: unknown variable 'ww'`<br>`4:20: cannot assign to parameter 'a' (parameters are read-only)` | PASS |
| errors_in_header_and_body | bad | `f(x: int) requires zz > 0 returns r: int ensures ww > 0 { r = vv; }` | `1:20: unknown variable 'zz'`<br>`1:50: unknown variable 'ww'`<br>`1:63: unknown variable 'vv'` | PASS |
| no_cascade_after_unknown_variable | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r = zz + 1;\n}\n` | `3:7: unknown variable 'zz'` | PASS |
| no_cascade_in_condition | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  if (zz + 1 > a and b[zz] > 0) r = 1;\n}\n` | `3:7: unknown variable 'zz'`<br>`3:24: unknown variable 'zz'` | PASS |
| no_cascade_after_bad_operator | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r = b + 1;\n  s = r;\n}\n` | `3:9: operator '+' requires int operands, found int[] and int`<br>`4:7: type mismatch in assignment to 's': expected int[], found int` | PASS |
| no_cascade_unknown_lhs | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  zz = b;\n}\n` | `3:3: unknown variable 'zz'` | PASS |
| no_cascade_after_unknown_call | bad | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n  r = g(1) + g(2);\n}\n` | `3:7: unknown function 'g'`<br>`3:14: unknown function 'g'` | PASS |
| syntax_errors_come_first | bad | `f() returns r: int ensures true r = ;` | `syntax: 1:37: expected expression, found ';'` | PASS |
| syntax_error_hides_semantic_errors | bad | `f() returns r: int ensures zz > 0 r = ;` | `syntax: 1:39: expected expression, found ';'` | PASS |
| gcd | good | `gcd(x: int, y: int) requires x > 0 and y > 0 returns r: int ensures r > 0 uses a: int, b: int\n{ a = x; b = y; while (a != b) invariant a > 0 and b > 0 { if (a > b) a = a - b; else b = b - a; } r = a; }` | `(no errors)` | PASS |
| divide_and_use | good | `divide(a: int, b: int) requires b > 0 returns q: int, r: int ensures a == q * b + r and 0 <= r and r < b\n{ q = 0; r = a; while (r >= b) { r = r - b; q = q + 1; } }\nuseDivide(n: int) returns s: int ensures true uses q: int, r: int { q, r = d…(265 chars)` | `(no errors)` | PASS |
| sum_of_array | good | `sum(a: int[]) returns s: int ensures s >= 0 uses i: int\n{ s = 0; i = 0; while (i < length(a)) { s = s + a[i]; i = i + 1; } }` | `(no errors)` | PASS |
| array_update_copy | good | `fill(a: int[], n: int) returns b: int[] ensures length(b) == length(a) uses i: int\n{ b = a; i = 0; while (i < n) invariant 0 <= i { b[i] = a[i + 1] * 2; i = i + 1; } }` | `(no errors)` | PASS |
| formulas_and_quantifiers | good | `sorted(a: int[], n: int) => forall (i: int \| i < 0 or i + 1 >= n or a[i] <= a[i + 1])\nhasZero(a: int[]) => exists (i: int \| i >= 0 and i < length(a) and a[i] == 0)\nfirst(a: int[]) requires sorted(a, length(a)) returns r: int ensures not has…(271 chars)` | `(no errors)` | PASS |
| array_quantifier_variable | good | `f(a: int[]) returns r: int ensures exists (c: int[] \| length(c) == length(a)) r = 0;` | `(no errors)` | PASS |
| implication_in_conditions | good | `f(x: int) returns r: int { if (x > 0 -> x > 1 -> x > 2) r = x; else r = 0; }` | `(no errors)` | PASS |
| nested_blocks_and_else_if | good | `f(x: int) returns r: int { { { r = 0; } } if (x > 0) r = 1; else if (x < 0) r = -1; else r = 2; }` | `(no errors)` | PASS |
| read_before_assignment_is_allowed | good | `f() returns r: int uses t: int { r = t + 1; }` | `(no errors)` | PASS |
| unused_variables_are_allowed | good | `f(a: int, b: int[]) returns r: int, s: int[] uses i: int, j: int { r = 1; }` | `(no errors)` | PASS |
| missing_ensures_is_allowed | good | `f() returns r: int r = 1;` | `(no errors)` | PASS |
| typed_precedence | typed | `1 + 2 * 3` | `(+:int 1:int (*:int 2:int 3:int))` | PASS |
| typed_unary_minus | typed | `a - -c` | `(-:int a:int (Neg:int c:int))` | PASS |
| typed_int_min | typed | `-2147483648` | `(Neg:int 2147483648:int)` | PASS |
| typed_index | typed | `b[a + 1]` | `(Index:int b (+:int a:int 1:int))` | PASS |
| typed_length | typed | `length(b) - 1` | `(-:int (Call:int length b:int[]) 1:int)` | PASS |
| typed_return_variable | typed | `r` | `r:int` | PASS |
| typed_user_call_known | typed | `g(x: int, v: int[]) ...; r = g(a, b) * 2` | `(*:int (Call:int g a:int b:int[]) 2:int)` | PASS |
| typed_call_returning_array_in_length | typed | `h(v: int[]) ...; r = length(h(b))` | `(Call:int length (Call:int[] h b:int[]))` | PASS |
| typed_condition | typed | `(a > 0 or b[0] == c) and not a < c` | `(And:bool (Or:bool (>:bool a:int 0:int) (==:bool (Index:int b 0:int) c:int)) (Not:bool (<:bool a:int c:int)))` | PASS |
| typed_implication | typed | `a > 0 -> c > 0` | `(Implies:bool (>:bool a:int 0:int) (>:bool c:int 0:int))` | PASS |
| typed_bool_constants | typed | `true and not false` | `(And:bool (BoolConst:bool true) (Not:bool (BoolConst:bool false)))` | PASS |
| typed_forall | typed | `forall (i: int \| i < 0 or i >= length(b) or b[i] >= 0)` | `(Forall:bool i int (Or:bool (Or:bool (<:bool i:int 0:int) (>=:bool i:int (Call:int length b:int[]))) (>=:bool (Index:int b i:int) 0:int)))` | PASS |
| typed_exists_array_variable | typed | `exists (d: int[] \| length(d) == a)` | `(Exists:bool d int[] (==:bool (Call:int length d:int[]) a:int))` | PASS |
| typed_formula_ref | typed | `p(x: int, v: int[]) => ...; ensures p(a, b)` | `(FormulaRef:bool p a:int b:int[])` | PASS |
| typed_function_with_statements | typed | `f(a: int[]) returns b: int[] { b = a; b[0] = 1; }` | `(Program (Function f (Params (VarDef a int[])) (Returns (VarDef b int[])) (Block (Assign:int[] b a:int[]) (ArrayAssign:int[] b 0:int 1:int))))` | PASS |
| typed_tuple_assignment | typed | `d() returns q: int, r: int q = 1;\nf() returns a: int, b: int { a, b = d(); }` | `(Program (Function d (Params) (Returns (VarDef q int) (VarDef r int)) (Assign:int q 1:int)) (Function f (Params) (Returns (VarDef a int) (VarDef b int)) (Block (TupleAssign (Targets a:int b:int) (Call:(int,int) d)))))` | PASS |
| typed_while_and_if | typed | `f(x: int) returns r: int { while (r < x) invariant r >= 0 r = r + 1; }` | `(Program (Function f (Params (VarDef x int)) (Returns (VarDef r int)) (Block (While (<:bool r:int x:int) (Invariant (>=:bool r:int 0:int)) (Assign:int r (+:int r:int 1:int))))))` | PASS |
| typed_error_nodes_are_marked | typed | `f() returns r: int r = zz;` | `ERR 1:24: unknown variable 'zz'\n(Program (Function f (Params) (Returns (VarDef r int)) (Assign:int r zz:error)))` | PASS |
| json_function_has_signature | annot | `inc(x: int) returns r: int r = x + 1;` | `yes` | PASS |
| json_var_has_type_and_role | annot | `inc(x: int) returns r: int r = x + 1;` | `yes` | PASS |
| json_binary_has_type_at_operator | annot | `inc(x: int) returns r: int r = x + 1;` | `yes` | PASS |
| json_vardef_has_declared_type_and_role | annot | `inc(x: int) returns r: int r = x + 1;` | `yes` | PASS |
| json_builtin_call | annot | `f(a: int[]) returns r: int r = length(a);` | `yes` | PASS |
| json_formula_and_quantifier | annot | `p(x: int) => forall (i: int \| i > x)` | `yes` | PASS |
| json_tuple_type | annot | `d() returns q: int, r: int[] q = 1;\nf() returns a: int, b: int[] { a, b = d(); }` | `yes` | PASS |
| tree_format | annot | `inc(x: int) returns r: int r = x;` | `Program\n  Function inc  (int)->(int)\n    Params\n      VarDef x: int\n    Returns\n      VarDef r: int\n    Assign r -> int\n      Var x -> int\n` | PASS |
| function_table | annot | `g(...) ...; p(...) => ...` | `g function 2 params, 2 returns, at 1:1\np formula 1 params, 0 returns, at 2:1\n` | PASS |
| every_expression_node_has_a_type | annot | `a program with ~30 expression nodes` | `ok=yes typed=36/36` | PASS |
| error_position_multiline | bad | `f() returns r: int\n{\n  r =\n     zz;\n}\n` | `4:6: unknown variable 'zz'` | PASS |
| error_position_crlf | bad | `f() returns r: int\r\n{\r\n  r = zz;\r\n}\r\n` | `3:7: unknown variable 'zz'` | PASS |
| error_position_after_non_ascii_comment | bad | `// привет\nf() returns r: int r = zz;` | `2:24: unknown variable 'zz'` | PASS |
| diagnostic_text_with_caret | annot | `f() returns r: int r = zz;` | `a.fn:1:24: error: unknown variable 'zz'\n    f() returns r: int r = zz;\n                           ^\n` | PASS |
| every_prefix_of_good_programs | robust | `all prefixes of 3 programs (548 runs, 7 reach type checking)` | `no crash, errors sorted by position` | PASS |
| random_renames_of_identifiers | robust | `3000 programs with 1-3 random renames` | `no crash, errors sorted by position` | PASS |
| deep_expression_150 | robust | `150 x '(' around an unknown variable` | `1 semantic error` | PASS |
| long_left_chain_150_errors | robust | `1 + b + b + ... (150 times)` | `150 semantic error` | PASS |
