# HW2 report

Parser: LL(1), recursive descent; tokens come from the HW1 lexer (DFA built from `../lexer/data/tokens.txt`), every token carries line:column. Errors are collected in panic mode.

## Tests: 152/152 passed (good 74, bad 73, robust 5)

`good` — program/fragment must parse and give the expected AST (S-expression); `bad` — must give exactly the expected diagnostics (`line:col: message`); `robust` — parser must terminate without crashing on broken input.

| name | kind | input | expected | result |
|---|---|---|---|---|
| arith_mul_over_add | good | `1 + 2 * 3` | `(+ 1 (* 2 3))` | PASS |
| arith_mul_then_add | good | `1 * 2 + 3` | `(+ (* 1 2) 3)` | PASS |
| arith_parens | good | `(1 + 2) * 3` | `(* (+ 1 2) 3)` | PASS |
| arith_div_over_sub | good | `a - b / c` | `(- a (/ b c))` | PASS |
| arith_sub_left_assoc | good | `a - b - c` | `(- (- a b) c)` | PASS |
| arith_div_left_assoc | good | `a / b / c` | `(/ (/ a b) c)` | PASS |
| arith_sub_add_left_assoc | good | `a - b + c` | `(+ (- a b) c)` | PASS |
| arith_mul_div_left_assoc | good | `a * b / c * d` | `(* (/ (* a b) c) d)` | PASS |
| unary_minus_over_mul | good | `-a * b` | `(* (Neg a) b)` | PASS |
| unary_minus_rhs | good | `a * -b` | `(* a (Neg b))` | PASS |
| unary_minus_double | good | `- - a` | `(Neg (Neg a))` | PASS |
| unary_minus_after_binary | good | `a - -b` | `(- a (Neg b))` | PASS |
| call_args | good | `f(1, g(2), x + 1)` | `(Call f 1 (Call g 2) (+ x 1))` | PASS |
| call_no_args | good | `f()` | `(Call f)` | PASS |
| array_read | good | `a[i + 1] * 2` | `(* (Index a (+ i 1)) 2)` | PASS |
| array_nested_index | good | `a[b[0]]` | `(Index a (Index b 0))` | PASS |
| builtin_length | good | `length(a) - 1` | `(- (Call length a) 1)` | PASS |
| cond_and_over_or_1 | good | `a > 0 and b > 0 or c > 0` | `(Or (And (> a 0) (> b 0)) (> c 0))` | PASS |
| cond_and_over_or_2 | good | `a > 0 or b > 0 and c > 0` | `(Or (> a 0) (And (> b 0) (> c 0)))` | PASS |
| cond_not_over_and | good | `not a > 0 and b > 0` | `(And (Not (> a 0)) (> b 0))` | PASS |
| cond_not_not | good | `not not true` | `(Not (Not (BoolConst true)))` | PASS |
| cond_and_left_assoc | good | `a > 0 and b > 0 and c > 0` | `(And (And (> a 0) (> b 0)) (> c 0))` | PASS |
| cond_or_left_assoc | good | `a > 0 or b > 0 or c > 0` | `(Or (Or (> a 0) (> b 0)) (> c 0))` | PASS |
| cond_implies_right_assoc | good | `a > 0 -> b > 0 -> c > 0` | `(Implies (> a 0) (Implies (> b 0) (> c 0)))` | PASS |
| cond_implies_lowest | good | `a > 0 or b > 0 -> c > 0` | `(Implies (Or (> a 0) (> b 0)) (> c 0))` | PASS |
| cond_implies_unicode_arrow | good | `a > 0 → b > 0` | `(Implies (> a 0) (> b 0))` | PASS |
| cond_compare_== | good | `a == b` | `(== a b)` | PASS |
| cond_compare_!= | good | `a != b` | `(!= a b)` | PASS |
| cond_compare_<= | good | `a <= b` | `(<= a b)` | PASS |
| cond_compare_>= | good | `a >= b` | `(>= a b)` | PASS |
| cond_compare_< | good | `a < b` | `(< a b)` | PASS |
| cond_compare_> | good | `a > b` | `(> a b)` | PASS |
| cond_paren_arith | good | `(a + b) > c` | `(> (+ a b) c)` | PASS |
| cond_paren_bool | good | `(a > 0) and (b > 0)` | `(And (> a 0) (> b 0))` | PASS |
| cond_paren_double | good | `((a > 0))` | `(> a 0)` | PASS |
| cond_paren_or_then_and | good | `(a > 0 or b > 0) and c > 0` | `(And (Or (> a 0) (> b 0)) (> c 0))` | PASS |
| cond_paren_arith_both_sides | good | `(a) - 1 > (b)` | `(> (- a 1) b)` | PASS |
| cond_not_paren | good | `not (a > 0)` | `(Not (> a 0))` | PASS |
| pred_forall | good | `forall (i: int \| i > 0)` | `(Forall i int (> i 0))` | PASS |
| pred_exists_array | good | `exists (a: int[] \| length(a) == 0)` | `(Exists a int[] (== (Call length a) 0))` | PASS |
| pred_formula_ref | good | `p(x, 1)` | `(FormulaRef p x 1)` | PASS |
| pred_formula_ref_and_call | good | `p(x) and f(x) > 0` | `(And (FormulaRef p x) (> (Call f x) 0))` | PASS |
| pred_not_formula_ref | good | `not p(x) and q()` | `(And (Not (FormulaRef p x)) (FormulaRef q))` | PASS |
| pred_or_and_priority | good | `a > 0 or b > 0 and c > 0` | `(Or (> a 0) (And (> b 0) (> c 0)))` | PASS |
| pred_nested_quantifiers | good | `forall (i: int \| exists (j: int \| j > i)) and true` | `(And (Forall i int (Exists j int (> j i))) (BoolConst true))` | PASS |
| dangling_else_binds_to_nearest_if | good | `f(x: int, y: int) returns z: int ensures true if (x > 0) if (y < 0) z = 1; else z = 5;` | `outer if kids=2, inner if kids=3` | PASS |
| function_structure_optional_parts | good | `f(a: int[], n: int) returns r: int, b: int[] uses i: int, j: int { r = 0; }` | `name=f requires=no ensures=no params=2 param0=int[] returns=2 locals=2 body=yes` | PASS |
| formulas_without_semicolon | good | `pos(x: int) => x > 0  neg(x: int) => x < 0 same() => true` | `(Program (Formula pos (Params (VarDef x int)) (> x 0)) (Formula neg (Params (VarDef x int)) (< x 0)) (Formula same (Params) (BoolConst true)))` | PASS |
| all_statement_kinds | good | `f(a: int[]) returns x: int, y: int ensures true { a2 = 1; a[x + 1] = 2; x, y = g(1); assert x > 0; assume y > 0; while (x > 0) invariant x >= 0 x = x - 1; }` | `(Program (Function f (Params (VarDef a int[])) (Returns (VarDef x int) (VarDef y int)) (Ensures (BoolConst true)) (Block (Assign a2 1) (ArrayAssign a (+ x 1) 2) (TupleAssign (Targets x y) (Call g 1)) (Assert (> x 0)) (Assume (> y 0)) (While (> x 0) (Invariant (>= x 0)) (Assign x (- x 1))))))` | PASS |
| single_assign_from_call_is_plain_assign | good | `f() returns r: int ensures true r = g(1);` | `(Program (Function f (Params) (Returns (VarDef r int)) (Ensures (BoolConst true)) (Assign r (Call g 1))))` | PASS |
| else_if_chain | good | `f() returns r: int ensures true if (true) r = 1; else if (false) r = 2; else r = 3;` | `(Program (Function f (Params) (Returns (VarDef r int)) (Ensures (BoolConst true)) (If (BoolConst true) (Assign r 1) (If (BoolConst false) (Assign r 2) (Assign r 3)))))` | PASS |
| empty_block | good | `f() returns r: int ensures true { }` | `(Program (Function f (Params) (Returns (VarDef r int)) (Ensures (BoolConst true)) (Block)))` | PASS |
| node_positions_multiline | good | `f() returns r: int ensures true\n{\n  r = 1 +\n      2;\n}\n` | `function 1:1, r 3:3, + 3:9, 2 4:7` | PASS |
| crlf_error_position | bad | `f() returns r: int ensures true\r\n{\r\n  r = ;\r\n}\r\n` | `3:7: expected expression, found ';'` | PASS |
| lone_cr_error_position | bad | `f() returns r: int ensures true\r{\r  r = ;\r}\r` | `3:7: expected expression, found ';'` | PASS |
| json_output_has_kind_position_type | good | `zero() returns r: int ensures true r = 0;` | `Program, Number 0 at 1:40, type int present` | PASS |
| tree_output_format | good | `f() returns r: int ensures true r = a[i] + 1;` | `Assign r / Binary + / Index a / Var i / Number 1 (2 spaces per level)` | PASS |
| format_diagnostic_with_caret | bad | `f() returns r: int ensures true r = ;\n` | `a.fn:1:37: error: expected expression, found ';' + source line + caret under col 37` | PASS |
| empty_input | bad |  | `1:1: empty program: expected at least one function or formula` | PASS |
| only_whitespace_and_comment | bad | `  \n// nothing\n` | `3:1: empty program: expected at least one function or formula` | PASS |
| unclosed_paren_params | bad | `f(x: int returns r: int ensures true r = x;` | `1:10: expected ')' after parameters, found 'returns'` | PASS |
| unclosed_paren_expr | bad | `f() returns r: int ensures true r = (1 + 2;` | `1:43: expected ')' to close '(' opened at 1:37, found ';'` | PASS |
| unclosed_brace | bad | `f() returns r: int ensures true { r = 1;` | `1:41: expected '}' to close '{' opened at 1:33, found end of input` | PASS |
| extra_closing_brace | bad | `f() returns r: int ensures true r = 1; }` | `1:40: expected function or formula name, found '}'` | PASS |
| extra_number_in_statement | bad | `f() returns r: int ensures true r = 1 2;` | `1:39: expected ';' after assignment, found '2'` | PASS |
| assign_instead_of_eq_in_condition | bad | `f(x: int) returns r: int ensures true if (x = 1) r = 0;` | `1:45: expected comparison operator (==, !=, <, <=, >, >=), found '=' (did you mean '=='?)` | PASS |
| wrong_operator_eq_lt | bad | `f() returns r: int ensures true r =< 1;` | `1:36: expected expression, found '<'` | PASS |
| expression_statement | bad | `f() returns r: int ensures true r + 1;` | `1:35: expected '=' in assignment, found '+'` | PASS |
| unclosed_bracket_index | bad | `f() returns r: int ensures true r = a[1;` | `1:40: expected ']' to close '[' opened at 1:38, found ';'` | PASS |
| unclosed_paren_call | bad | `f() returns r: int ensures true r = g(1, 2;` | `1:43: expected ')' to close '(' opened at 1:38, found ';'` | PASS |
| if_without_parens | bad | `f() returns r: int ensures true if r > 0 r = 1;` | `1:36: expected '(' after 'if', found 'r'` | PASS |
| while_without_parens | bad | `f() returns r: int ensures true while x > 0 r = 1;` | `1:39: expected '(' after 'while', found 'x'` | PASS |
| vardef_without_colon | bad | `f(x int) returns r: int ensures true r = 1;` | `1:5: expected ':' after variable name, found 'int'` | PASS |
| assign_to_literal | bad | `f() returns r: int ensures true 1 = r;` | `1:33: expected statement, found '1'` | PASS |
| missing_body | bad | `f() returns r: int ensures true` | `1:32: expected statement, found end of input` | PASS |
| quantifier_without_pipe | bad | `f() returns r: int ensures forall (i: int i > 0) r = 1;` | `1:43: expected '\|' after quantifier variable, found 'i'` | PASS |
| double_operator | bad | `f() returns r: int ensures true r = 1 + * 2;` | `1:41: expected expression, found '*'` | PASS |
| missing_final_semicolon | bad | `f() returns r: int ensures true r = 1 + 2` | `1:42: expected ';' after assignment, found end of input` | PASS |
| assert_without_predicate | bad | `f() returns r: int ensures true { assert ; }` | `1:42: expected predicate, found ';'` | PASS |
| definition_without_name | bad | `() returns r: int ensures true r = 1;` | `1:1: expected function or formula name, found '('` | PASS |
| empty_array_index | bad | `f() returns r: int ensures true r = a[];` | `1:39: expected expression, found ']'` | PASS |
| dangling_else_at_eof | bad | `f() returns r: int ensures true if (x > 0) r = 1; else` | `1:55: expected statement, found end of input` | PASS |
| two_dimensional_index | bad | `f() returns r: int ensures true r = a[1][2];` | `1:41: expected ';' after assignment, found '['` | PASS |
| implication_in_ensures | bad | `f() returns r: int ensures a > 0 -> r > 0 r = 1;` | `1:34: implication is allowed only in conditions, not in predicates, found '->'` | PASS |
| unicode_implication_in_ensures | bad | `f() returns r: int ensures a > 0 → r > 0 r = 1;` | `1:34: implication is allowed only in conditions, not in predicates, found '→'` | PASS |
| implication_in_requires | bad | `f() requires a > 0 -> r > 0 returns r: int ensures true r = 1;` | `1:20: implication is allowed only in conditions, not in predicates, found '->'` | PASS |
| implication_in_formula | bad | `p(x: int) => x > 0 -> x > 1` | `1:20: implication is allowed only in conditions, not in predicates, found '->'` | PASS |
| implication_in_quantifier_body | bad | `f() returns r: int ensures forall (i: int \| i > 0 -> i > 1) r = 1;` | `1:51: implication is allowed only in conditions, not in predicates, found '->'` | PASS |
| implication_in_parenthesised_predicate | bad | `f() returns r: int ensures (a > 0 -> r > 0) r = 1;` | `1:35: expected ')' to close '(' opened at 1:28, found '->'` | PASS |
| semicolon_after_formula | bad | `p(x: int) => x > 0;` | `1:19: expected function or formula name, found ';'` | PASS |
| lexical_error_at_sign | bad | `f() returns r: int ensures true r = 1 @ 2;` | `1:39: unexpected character '@'`<br>`1:41: expected ';' after assignment, found '2'` | PASS |
| underscore_in_identifier | bad | `f(a_b: int) returns r: int ensures true r = 1;` | `1:4: unexpected character '_' (identifiers may contain only letters and digits)`<br>`1:5: expected ':' after variable name, found 'b'` | PASS |
| recovery_inside_block | bad | `f() returns r: int ensures true {\n r = ;\n r = 2;\n r = 1 + ;\n r = 3;\n r = * 3;\n}\n` | `3 errors at lines 2 4 6; statements kept=2` | PASS |
| recovery_next_definition | bad | `f() returns\n{ }\ng() returns r: int ensures true r = 1;` | `1 error; definitions kept=1 (g)` | PASS |
| lexer_contract_token_names | good | `token names the parser relies on (44) vs DFA names` | `all present` | PASS |
| tab_counts_as_one_column | bad | `f()\treturns r: int ensures true r = ;` | `1:37: expected expression, found ';'` | PASS |
| column_counts_bytes_not_chars | bad | `f() returns r: int ensures true r = é;` | `1:37: unexpected characters 'é'`<br>`1:39: expected expression, found ';'` | PASS |
| int_00_is_two_tokens | bad | `f() returns r: int ensures true r = 00;` | `1:38: expected ';' after assignment, found '0'` | PASS |
| int_then_letters | bad | `f() returns r: int ensures true r = 1a;` | `1:38: expected ';' after assignment, found 'a'` | PASS |
| arrow_is_not_assign | bad | `f() returns r: int ensures true r => 1;` | `1:35: expected '=' in assignment, found '=>'` | PASS |
| triple_eq_is_eq_then_assign | bad | `f() returns r: int ensures true r === 1;` | `1:35: expected '=' in assignment, found '=='` | PASS |
| underscore_splits_identifier | bad | `f() returns r: int ensures true r = a_b;` | `1:38: unexpected character '_' (identifiers may contain only letters and digits)`<br>`1:39: expected ';' after assignment, found 'b'` | PASS |
| keyword_function_is_not_a_header | bad | `function f() returns r: int ensures true r = 1;` | `1:1: expected function or formula name, found 'function' ('function' is a reserved word)` | PASS |
| keyword_as_variable_name | bad | `f() returns length: int ensures true length = 1;` | `1:13: expected identifier as variable name, found 'length' ('length' is a reserved word)` | PASS |
| keyword_prefix_is_identifier | good | `iffy() returns functions: int ensures true functions = 1;` | `(Program (Function iffy (Params) (Returns (VarDef functions int)) (Ensures (BoolConst true)) (Assign functions 1)))` | PASS |
| formula_is_not_keyword | good | `formula() => true` | `(Program (Formula formula (Params) (BoolConst true)))` | PASS |
| comment_cyrillic_own_line | good | `// привет, мир\nf() returns r: int ensures true r = 1;` | `(Program (Function f (Params) (Returns (VarDef r int)) (Ensures (BoolConst true)) (Assign r 1)))` | PASS |
| comment_cyrillic_after_code | good | `f() returns r: int ensures true\n{\n  r = 1; // считаем: x = 1\n}\n` | `(Program (Function f (Params) (Returns (VarDef r int)) (Ensures (BoolConst true)) (Block (Assign r 1))))` | PASS |
| comment_ascii_words_after_non_ascii | good | `f() returns r: int ensures true r = 1; // naïve code ) ( { ;\n` | `(Program (Function f (Params) (Returns (VarDef r int)) (Ensures (BoolConst true)) (Assign r 1)))` | PASS |
| comment_arrow_is_not_implication | good | `f() returns r: int ensures true r = 1; // a → b -> c\n` | `(Program (Function f (Params) (Returns (VarDef r int)) (Ensures (BoolConst true)) (Assign r 1)))` | PASS |
| comment_non_ascii_lone_cr | good | `// а\rf() returns r: int ensures true r = 1;` | `(Program (Function f (Params) (Returns (VarDef r int)) (Ensures (BoolConst true)) (Assign r 1)))` | PASS |
| comment_non_ascii_at_eof | good | `f() returns r: int ensures true r = 1; //б` | `(Program (Function f (Params) (Returns (VarDef r int)) (Ensures (BoolConst true)) (Assign r 1)))` | PASS |
| comment_non_ascii_on_every_line | good | `// а\nf() returns // б\nr: int ensures true // в\n r = 1; // г\n` | `(Program (Function f (Params) (Returns (VarDef r int)) (Ensures (BoolConst true)) (Assign r 1)))` | PASS |
| non_ascii_after_single_slash_is_error | bad | `f() returns r: int ensures true r = 1; /é` | `1:40: expected function or formula name, found '/'`<br>`1:41: unexpected characters 'é'` | PASS |
| non_ascii_in_code_then_comment | bad | `f() returns r: int ensures true r = é; // é\n` | `1:37: unexpected characters 'é'`<br>`1:39: expected expression, found ';'` | PASS |
| non_ascii_on_next_line_after_comment | bad | `f() returns r: int ensures true // c\nr = é;` | `2:5: unexpected characters 'é'`<br>`2:7: expected expression, found ';'` | PASS |
| arrays | good | `sum(a: int[]) returns s: int\n  ensures s >= 0\n  uses i: int\n{\n  s = 0;\n  i = 0;\n  while (i < length(a)) {\n    s = s + a[i];\n    i = i + 1;\n  }\n}\n\nfill(a: int[], n: int) returns b: int[]\n  ensures length(b) == length(a)\n  uses i: int\n{\n  b =…(324 chars)` | `(Program (Function sum (Params (VarDef a int[])) (Returns (VarDef s int)) (Ensures (>= s 0)) (Locals (VarDef i int)) (Block (Assign s 0) (Assign i 0) (While (< i (Call length a)) (Block (Assign s (+ s (Index a i))) (Assign i (+ i 1)))))) (Function fill (Params (VarDef a int[]) (VarDef n int)) (Returns (VarDef b int[])) (Ensures (== (Call length b) (Call length a))) (Locals (VarDef i int)) (Block (…(533 chars)` | PASS |
| comment_eof | good | `h() returns r: int ensures true r = 1; // comment at EOF, no trailing newline` | `(Program (Function h (Params) (Returns (VarDef r int)) (Ensures (BoolConst true)) (Assign r 1)))` | PASS |
| divide | good | `divide(a: int, b: int)\n  requires b > 0\n  returns q: int, r: int\n  ensures a == q * b + r and 0 <= r and r < b\n{\n  q = 0;\n  r = a;\n  while (r >= b) { r = r - b; q = q + 1; }\n}\n\n// tuple assignment\nuseDivide(n: int) returns s: int\n  ensures …(307 chars)` | `(Program (Function divide (Params (VarDef a int) (VarDef b int)) (Requires (> b 0)) (Returns (VarDef q int) (VarDef r int)) (Ensures (And (And (== a (+ (* q b) r)) (<= 0 r)) (< r b))) (Block (Assign q 0) (Assign r a) (While (>= r b) (Block (Assign r (- r b)) (Assign q (+ q 1)))))) (Function useDivide (Params (VarDef n int)) (Returns (VarDef s int)) (Ensures (BoolConst true)) (Locals (VarDef q int)…(491 chars)` | PASS |
| formulas | good | `sorted(a: int[], n: int) =>\n  forall (i: int \| i < 0 or i + 1 >= n or a[i] <= a[i + 1])\n\npositive(x: int) => x > 0\n\nhasZero(a: int[]) => exists (i: int \| i >= 0 and i < length(a) and a[i] == 0)\n\nfirst(a: int[]) requires sorted(a, length(a))…(349 chars)` | `(Program (Formula sorted (Params (VarDef a int[]) (VarDef n int)) (Forall i int (Or (Or (< i 0) (>= (+ i 1) n)) (<= (Index a i) (Index a (+ i 1)))))) (Formula positive (Params (VarDef x int)) (> x 0)) (Formula hasZero (Params (VarDef a int[])) (Exists i int (And (And (>= i 0) (< i (Call length a))) (== (Index a i) 0)))) (Function first (Params (VarDef a int[])) (Requires (And (FormulaRef sorted a …(578 chars)` | PASS |
| gcd | good | `// Euclid gcd: while with invariant and local variables\ngcd(x: int, y: int)\n  requires x > 0 and y > 0\n  returns r: int\n  ensures r > 0\n  uses a: int, b: int\n{\n  a = x;\n  b = y;\n  while (a != b)\n    invariant a > 0 and b > 0\n  {\n    if (a >…(286 chars)` | `(Program (Function gcd (Params (VarDef x int) (VarDef y int)) (Requires (And (> x 0) (> y 0))) (Returns (VarDef r int)) (Ensures (> r 0)) (Locals (VarDef a int) (VarDef b int)) (Block (Assign a x) (Assign b y) (While (!= a b) (Invariant (And (> a 0) (> b 0))) (Block (If (> a b) (Assign a (- a b)) (Assign b (- b a))))) (Assign r a))))` | PASS |
| implies_in_conditions | good | `g(x: int) returns r: int ensures true\n{\n  if (x > 0 → x > 1 → x > 2) r = x; else r = 0;\n  if (x > 0 -> x > 1) r = 1;\n}\n` | `(Program (Function g (Params (VarDef x int)) (Returns (VarDef r int)) (Ensures (BoolConst true)) (Block (If (Implies (> x 0) (Implies (> x 1) (> x 2))) (Assign r x) (Assign r 0)) (If (Implies (> x 0) (> x 1)) (Assign r 1)))))` | PASS |
| minimal | good | `zero() returns r: int ensures true r = 0;\n` | `(Program (Function zero (Params) (Returns (VarDef r int)) (Ensures (BoolConst true)) (Assign r 0)))` | PASS |
| precedence | good | `p(a: int, b: int, c: int) returns r: int\n  ensures (a + b) * c > a - b - c and a / b / c <= -a * -b\n{\n  r = 1 + 2 * 3 - 4 / 2 + -a * (b - c);\n  if ((a + b) > c and (a < b or not b < c) -> a == 0) r = 0;\n}\n` | `(Program (Function p (Params (VarDef a int) (VarDef b int) (VarDef c int)) (Returns (VarDef r int)) (Ensures (And (> (* (+ a b) c) (- (- a b) c)) (<= (/ (/ a b) c) (* (Neg a) (Neg b))))) (Block (Assign r (+ (- (+ 1 (* 2 3)) (/ 4 2)) (* (Neg a) (- b c)))) (If (Implies (And (> (+ a b) c) (Or (< a b) (Not (< b c)))) (== a 0)) (Assign r 0)))))` | PASS |
| statements | good | `f(x: int, y: int) returns z: int ensures true\n{\n  assume x >= 0;\n  if (x > 0) if (y < 0) z = 1; else z = 5;\n  if (x == 0) { z = 0; } else { z = -1; }\n  { { z = z + 1; } }\n  while (false) z = z;\n  assert z != 100 or not (x < y);\n}\n` | `(Program (Function f (Params (VarDef x int) (VarDef y int)) (Returns (VarDef z int)) (Ensures (BoolConst true)) (Block (Assume (>= x 0)) (If (> x 0) (If (< y 0) (Assign z 1) (Assign z 5))) (If (== x 0) (Block (Assign z 0)) (Block (Assign z (Neg 1)))) (Block (Block (Assign z (+ z 1)))) (While (BoolConst false) (Assign z z)) (Assert (Or (!= z 100) (Not (< x y)))))))` | PASS |
| bad_condition | bad | `f(x: int) returns r: int ensures true\n{\n  if (x) r = 1;\n  while (x < ) r = 2;\n}\n` | `3:8: expected comparison operator (==, !=, <, <=, >, >=), found ')'`<br>`4:14: expected expression, found ')'` | PASS |
| bad_type | bad | `f(x: float) returns r: int ensures true r = 1;\n` | `1:6: expected type 'int' or 'int[]', found 'float'` | PASS |
| chained_comparison | bad | `f(x: int) returns r: int ensures x < 1 < 2 r = 1;\n` | `1:40: unexpected '<' (comparisons cannot be chained)` | PASS |
| else_without_if | bad | `f() returns r: int ensures true\n{\n  else r = 1;\n}\n` | `3:3: 'else' without matching 'if'` | PASS |
| empty | bad |  | `1:1: empty program: expected at least one function or formula` | PASS |
| extra_brace | bad | `f() returns r: int ensures true { r = 1; } }\n` | `1:44: expected function or formula name, found '}'` | PASS |
| extra_number | bad | `f() returns r: int ensures true r = 1; 42\n` | `1:40: expected function or formula name, found '42'` | PASS |
| formula_semicolon | bad | `positive(x: int) => x > 0;\n` | `1:26: expected function or formula name, found ';'` | PASS |
| garbage_tokens | bad | `f() returns r: int ensures true\n{\n  ) ) ) ;\n  r = 1;\n}\n` | `3:3: expected statement, found ')'` | PASS |
| implication_in_predicate | bad | `f(x: int) returns r: int\n  ensures x > 0 -> r > 0\n  r = x;\n` | `2:17: implication is allowed only in conditions, not in predicates, found '->'` | PASS |
| lexical_errors | bad | `f(my_var: int) returns r: int ensures true r = 1 @ 2;\n` | `1:5: unexpected character '_' (identifiers may contain only letters and digits)`<br>`1:6: expected ':' after variable name, found 'var'`<br>`1:50: unexpected character '@'` | PASS |
| missing_returns | bad | `f(x: int) ensures true { x = 1; }\n` | `1:11: expected 'returns' in function header, found 'ensures'` | PASS |
| missing_semicolon | bad | `f(x: int) returns r: int ensures true\n{\n  r = x\n  r = r + 1;\n}\n` | `4:3: expected ';' after assignment, found 'r'` | PASS |
| only_comment | bad | `  \n // only a comment\n` | `3:1: empty program: expected at least one function or formula` | PASS |
| quantifier_in_condition | bad | `f(x: int) returns r: int ensures true\n{\n  if (forall (i: int \| i > 0)) r = 1;\n}\n` | `3:7: quantifiers are allowed only in predicates, not in conditions` | PASS |
| recovery_next_def | bad | `f() returns r: int\n  ensures true\n{ r = 1; }\ng(x: int) returns\n{ }\nh() returns r: int ensures true r = 2;\n` | `5:1: expected identifier as variable name, found '{'` | PASS |
| several_errors | bad | `f(x: int) returns r: int ensures true\n{\n  r = ;\n  r = 1 + ;\n  r = 2;\n  r = * 3;\n}\n` | `3:7: expected expression, found ';'`<br>`4:11: expected expression, found ';'`<br>`6:7: expected expression, found '*'` | PASS |
| tuple_needs_call | bad | `f(x: int) returns a: int, b: int ensures true\n{\n  a, b = x + 1;\n}\n` | `3:12: tuple assignment requires a function call on the right side` | PASS |
| unclosed_brace | bad | `f(x: int) returns r: int ensures true\n{\n  r = x;\n` | `4:1: expected '}' to close '{' opened at 2:1, found end of input` | PASS |
| unclosed_paren_expr | bad | `f(x: int) returns r: int ensures true\n{\n  r = (x + 1;\n}\n` | `3:13: expected ')' to close '(' opened at 3:7, found ';'` | PASS |
| unclosed_paren_params | bad | `f(x: int returns r: int\n  ensures true\n  r = x;\n` | `1:10: expected ')' after parameters, found 'returns'` | PASS |
| wrong_operator | bad | `f(x: int) returns r: int ensures true\n{\n  if (x = 1) r = 0;\n  r == 1;\n}\n` | `3:9: expected comparison operator (==, !=, <, <=, >, >=), found '=' (did you mean '=='?)`<br>`4:5: expected '=' in assignment, found '=='` | PASS |
| every_prefix_of_good_programs | robust | `all prefixes of the good programs (1952 runs)` | `terminates, no crash` | PASS |
| random_mutations_of_good_programs | robust | `4000 good programs with 1-4 random edits` | `terminates, no crash` | PASS |
| random_token_sequences | robust | `6000 random sequences of up to 40 tokens` | `terminates, no crash` | PASS |
| deep_nesting_5000_is_error_not_crash | robust | `5000 x '(' / '{' / '-'` | `3 errors reported, no stack overflow` | PASS |
| moderate_nesting_150_ok | robust | `150 x '(' around 1` | `parses without errors` | PASS |

## Как устроены тесты `robust`

Эти тесты не сверяют результат с эталоном: они проверяют, что парсер не падает и не зависает (`parse(...)` возвращает управление) на испорченном вводе. Один «запуск» — один вызов `parse(...)` на одном тексте. Случайные тесты используют генератор с фиксированным начальным значением, поэтому каждый прогон одинаков.

- **Все префиксы хороших программ** (число запусков указано в таблице). Каждая хорошая программа режется на куски: первые 0 символов, первый 1, первые 2, и так до полной длины. Это имитирует обрыв ввода в любом месте: незакрытые скобки, обрыв посреди ключевого слова и т. д.
- **Случайные мутации** (4000 запусков). Берётся случайная хорошая программа, к ней применяется от 1 до 4 случайных правок: заменить символ, удалить от 1 до 5 символов или вставить символ. Символы берутся из набора мусора `(){};,=<>+-*/|:[]a1`, перевод строки, `!@_`.
- **Случайные последовательности токенов** (6000 запусков). Из пула слов (`f`, `x`, `1`, `(`, `if`, `while`, `returns`, `forall` и т. д.) склеивается до 40 случайных слов через пробел: получается абракадабра, но из корректных токенов.
- **Глубокая вложенность.** 5000 открывающих скобок `(`, `{` или минусов `-` подряд. Парсер должен вернуть ошибку, а не упасть от переполнения стека. Отдельно проверяется, что 150 скобок вокруг единицы разбираются без ошибок.
