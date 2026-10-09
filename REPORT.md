# HW1 report

## Automaton sizes

| Stage | States |
|---|---|
| NFA (Thompson) | 295 |
| DFA (subset construction, partial) | 124 |
| DFA before minimization (+ trap) | 125 |
| Minimal DFA (Hopcroft, incl. trap) | 122 |

Start state: 0. Trap state: 1 (non-accepting, loops to itself on all 128 symbols; bytes >= 128 go there too).

## Tokens (47)

| priority | name | skip | regex |
|---|---|---|---|
| 0 | WS | yes | `[ \t\r\n]+` |
| 1 | COMMENT | yes | `//[^\r\n]*` |
| 2 | KW_FUNCTION |  | `function` |
| 3 | KW_RETURNS |  | `returns` |
| 4 | KW_REQUIRES |  | `requires` |
| 5 | KW_ENSURES |  | `ensures` |
| 6 | KW_USES |  | `uses` |
| 7 | KW_IF |  | `if` |
| 8 | KW_ELSE |  | `else` |
| 9 | KW_WHILE |  | `while` |
| 10 | KW_INVARIANT |  | `invariant` |
| 11 | KW_TRUE |  | `true` |
| 12 | KW_FALSE |  | `false` |
| 13 | KW_NOT |  | `not` |
| 14 | KW_AND |  | `and` |
| 15 | KW_OR |  | `or` |
| 16 | KW_FORALL |  | `forall` |
| 17 | KW_EXISTS |  | `exists` |
| 18 | KW_LENGTH |  | `length` |
| 19 | KW_ASSERT |  | `assert` |
| 20 | KW_ASSUME |  | `assume` |
| 21 | KW_INT |  | `int` |
| 22 | IDENT |  | `[a-zA-Z][a-zA-Z0-9]*` |
| 23 | INT |  | `0\|[1-9][0-9]*` |
| 24 | OP_ARROW |  | `=>` |
| 25 | OP_IMPLIES |  | `->` |
| 26 | OP_EQ |  | `==` |
| 27 | OP_NEQ |  | `!=` |
| 28 | OP_LE |  | `<=` |
| 29 | OP_GE |  | `>=` |
| 30 | OP_ASSIGN |  | `=` |
| 31 | OP_LT |  | `<` |
| 32 | OP_GT |  | `>` |
| 33 | OP_LPAREN |  | `\(` |
| 34 | OP_RPAREN |  | `\)` |
| 35 | OP_LBRACKET |  | `\[` |
| 36 | OP_RBRACKET |  | `\]` |
| 37 | OP_LBRACE |  | `{` |
| 38 | OP_RBRACE |  | `}` |
| 39 | OP_COMMA |  | `,` |
| 40 | OP_SEMI |  | `;` |
| 41 | OP_COLON |  | `:` |
| 42 | OP_PLUS |  | `\+` |
| 43 | OP_MINUS |  | `-` |
| 44 | OP_STAR |  | `\*` |
| 45 | OP_SLASH |  | `/` |
| 46 | OP_PIPE |  | `\\|` |

## Tests: 93/93 passed (good 70, bad 23)

| name | kind | input | expected | result |
|---|---|---|---|---|
| good_empty | good | `` | `` | PASS |
| good_spaces | good | ` \t ` | `` | PASS |
| good_tabs | good | `\t\t\t` | `` | PASS |
| good_crlf | good | `\r\n\r\n` | `` | PASS |
| good_mixed_whitespace | good | ` \t\r\n \t\r\n` | `` | PASS |
| good_int_zero | good | `0` | `INT(0)` | PASS |
| good_int_multi_digit | good | `123` | `INT(123)` | PASS |
| good_int_max_like | good | `2147483647` | `INT(2147483647)` | PASS |
| good_ident | good | `gcd` | `IDENT(gcd)` | PASS |
| good_ident_digits | good | `a1b2c3` | `IDENT(a1b2c3)` | PASS |
| good_ident_upper | good | `Function` | `IDENT(Function)` | PASS |
| good_formula_is_not_keyword | good | `formula` | `IDENT(formula)` | PASS |
| good_keyword_function | good | `function` | `KW_FUNCTION(function)` | PASS |
| good_keyword_returns | good | `returns` | `KW_RETURNS(returns)` | PASS |
| good_keyword_requires | good | `requires` | `KW_REQUIRES(requires)` | PASS |
| good_keyword_ensures | good | `ensures` | `KW_ENSURES(ensures)` | PASS |
| good_keyword_uses | good | `uses` | `KW_USES(uses)` | PASS |
| good_keyword_while | good | `while` | `KW_WHILE(while)` | PASS |
| good_keyword_if | good | `if` | `KW_IF(if)` | PASS |
| good_keyword_else | good | `else` | `KW_ELSE(else)` | PASS |
| good_keyword_assert | good | `assert` | `KW_ASSERT(assert)` | PASS |
| good_keyword_assume | good | `assume` | `KW_ASSUME(assume)` | PASS |
| good_keyword_invariant | good | `invariant` | `KW_INVARIANT(invariant)` | PASS |
| good_keyword_length | good | `length` | `KW_LENGTH(length)` | PASS |
| good_keyword_int | good | `int` | `KW_INT(int)` | PASS |
| good_keyword_true | good | `true` | `KW_TRUE(true)` | PASS |
| good_keyword_false | good | `false` | `KW_FALSE(false)` | PASS |
| good_keyword_not | good | `not` | `KW_NOT(not)` | PASS |
| good_keyword_and | good | `and` | `KW_AND(and)` | PASS |
| good_keyword_or | good | `or` | `KW_OR(or)` | PASS |
| good_keyword_forall | good | `forall` | `KW_FORALL(forall)` | PASS |
| good_keyword_exists | good | `exists` | `KW_EXISTS(exists)` | PASS |
| good_keyword_prefix_is_ident | good | `functions iffy` | `IDENT(functions) IDENT(iffy)` | PASS |
| good_keyword_then_ident | good | `if iffy` | `KW_IF(if) IDENT(iffy)` | PASS |
| good_delimiters | good | `()[]{},;:` | `OP_LPAREN(() OP_RPAREN()) OP_LBRACKET([) OP_RBRACKET(]) OP_LBRACE({) OP_RBRACE(}) OP_COMMA(,) OP_SEMI(;) OP_COLON(:)` | PASS |
| good_arithmetic | good | `+ - * /` | `OP_PLUS(+) OP_MINUS(-) OP_STAR(*) OP_SLASH(/)` | PASS |
| good_comparison | good | `== != <= >= < >` | `OP_EQ(==) OP_NEQ(!=) OP_LE(<=) OP_GE(>=) OP_LT(<) OP_GT(>)` | PASS |
| good_assign_vs_eq | good | `a=b a==b` | `IDENT(a) OP_ASSIGN(=) IDENT(b) IDENT(a) OP_EQ(==) IDENT(b)` | PASS |
| good_maximal_munch_triple_eq | good | `===` | `OP_EQ(==) OP_ASSIGN(=)` | PASS |
| good_arrow_and_pipe | good | `a => b \| c` | `IDENT(a) OP_ARROW(=>) IDENT(b) OP_PIPE(\|) IDENT(c)` | PASS |
| good_implication_chain | good | `a -> b -> c` | `IDENT(a) OP_IMPLIES(->) IDENT(b) OP_IMPLIES(->) IDENT(c)` | PASS |
| good_minus_gt_separated | good | `x - >` | `IDENT(x) OP_MINUS(-) OP_GT(>)` | PASS |
| good_comment_skipped | good | `x = 1; // set x\ny = 2;` | `IDENT(x) OP_ASSIGN(=) INT(1) OP_SEMI(;) IDENT(y) OP_ASSIGN(=) INT(2) OP_SEMI(;)` | PASS |
| good_comment_eof_no_newline | good | `x = 1; // tail` | `IDENT(x) OP_ASSIGN(=) INT(1) OP_SEMI(;)` | PASS |
| good_comment_only | good | `//` | `` | PASS |
| good_comment_crlf | good | `a // c\r\nb` | `IDENT(a) IDENT(b)` | PASS |
| good_comment_lone_cr | good | `a // c\rb` | `IDENT(a) IDENT(b)` | PASS |
| good_slash_vs_comment | good | `a / b // c` | `IDENT(a) OP_SLASH(/) IDENT(b)` | PASS |
| good_array_type | good | `a: int[]` | `IDENT(a) OP_COLON(:) KW_INT(int) OP_LBRACKET([) OP_RBRACKET(])` | PASS |
| good_array_access | good | `a[i] = length(a);` | `IDENT(a) OP_LBRACKET([) IDENT(i) OP_RBRACKET(]) OP_ASSIGN(=) KW_LENGTH(length) OP_LPAREN(() IDENT(a) OP_RPAREN()) OP_SEMI(;)` | PASS |
| good_quantifier | good | `forall(i: int \| i >= 0)` | `KW_FORALL(forall) OP_LPAREN(() IDENT(i) OP_COLON(:) KW_INT(int) OP_PIPE(\|) IDENT(i) OP_GE(>=) INT(0) OP_RPAREN())` | PASS |
| good_formula_def | good | `pos(x: int) => x > 0` | `IDENT(pos) OP_LPAREN(() IDENT(x) OP_COLON(:) KW_INT(int) OP_RPAREN()) OP_ARROW(=>) IDENT(x) OP_GT(>) INT(0)` | PASS |
| good_function_header | good | `gcd(x: int, y: int)\n  returns r: int` | `IDENT(gcd) OP_LPAREN(() IDENT(x) OP_COLON(:) KW_INT(int) OP_COMMA(,) IDENT(y) OP_COLON(:) KW_INT(int) OP_RPAREN()) KW_RETURNS(returns) IDENT(r) OP_COLON(:) KW_INT(int)` | PASS |
| bad_ident_underscore | bad | `my_var` | `IDENT(my) ERR(_) IDENT(var)` | PASS |
| bad_leading_underscore | bad | `_x` | `ERR(_) IDENT(x)` | PASS |
| bad_non_ascii_latin1 | bad | `x = caf\xe9;` | `IDENT(x) OP_ASSIGN(=) IDENT(caf) ERR(\xe9) OP_SEMI(;)` | PASS |
| bad_unicode_arrow_utf8 | bad | `a \xe2\x86\x92 b` | `IDENT(a) ERR(\xe2) ERR(\x86) ERR(\x92) IDENT(b)` | PASS |
| bad_comment_cut_by_non_ascii | bad | `// \xe9` | `ERR(\xe9)` | PASS |
| bad_nul_byte | bad | `\x00` | `ERR(\x00)` | PASS |
| bad_lone_bang | bad | `!` | `ERR(!)` | PASS |
| bad_unused_ascii | bad | `@ # $ % ^ & ~ ` ? . ' "` | `ERR(@) ERR(#) ERR($) ERR(%) ERR(^) ERR(&) ERR(~) ERR(`) ERR(?) ERR(.) ERR(') ERR(")` | PASS |
| bad_double_amp | bad | `a && b` | `IDENT(a) ERR(&) ERR(&) IDENT(b)` | PASS |
| good_int_00_is_two_tokens | good | `00` | `INT(0) INT(0)` | PASS |
| good_int_01_is_two_tokens | good | `01` | `INT(0) INT(1)` | PASS |
| good_int_007_is_three_tokens | good | `007` | `INT(0) INT(0) INT(7)` | PASS |
| good_int_then_ident | good | `1a` | `INT(1) IDENT(a)` | PASS |
| good_whole_0 | good | `0` | `INT` | PASS |
| good_whole_7 | good | `7` | `INT` | PASS |
| good_whole_10 | good | `10` | `INT` | PASS |
| good_whole_123 | good | `123` | `INT` | PASS |
| good_whole_if | good | `if` | `KW_IF` | PASS |
| good_whole_iffy | good | `iffy` | `IDENT` | PASS |
| good_whole_a1 | good | `a1` | `IDENT` | PASS |
| good_whole_eq | good | `==` | `OP_EQ` | PASS |
| good_whole_arrow | good | `=>` | `OP_ARROW` | PASS |
| good_whole_implies | good | `->` | `OP_IMPLIES` | PASS |
| good_whole_comment_text | good | `// c` | `COMMENT` | PASS |
| good_whole_comment_empty | good | `//` | `COMMENT` | PASS |
| good_whole_whitespace | good | ` \t\r\n` | `WS` | PASS |
| bad_whole_00 | bad | `00` | `REJECT` | PASS |
| bad_whole_01 | bad | `01` | `REJECT` | PASS |
| bad_whole_007 | bad | `007` | `REJECT` | PASS |
| bad_whole_empty | bad | `` | `REJECT` | PASS |
| bad_whole_underscore | bad | `my_var` | `REJECT` | PASS |
| bad_whole_starts_with_digit_ident | bad | `1a` | `REJECT` | PASS |
| bad_whole_lone_bang | bad | `!` | `REJECT` | PASS |
| bad_whole_two_tokens | bad | `a b` | `REJECT` | PASS |
| bad_whole_minus_one | bad | `-1` | `REJECT` | PASS |
| bad_whole_triple_arrow | bad | `=>>` | `REJECT` | PASS |
| bad_whole_non_ascii | bad | `\xe9` | `REJECT` | PASS |
| bad_whole_utf8_arrow | bad | `\xe2\x86\x92` | `REJECT` | PASS |
| bad_whole_comment_with_newline | bad | `//x\ny` | `REJECT` | PASS |
| bad_whole_slash_space_slash | bad | `/ /` | `REJECT` | PASS |
