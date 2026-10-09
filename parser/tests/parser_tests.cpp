// Тесты парсера Funny (HW2) без внешних зависимостей. Запуск: make test в папке parser/
//
// Все эталоны лежат прямо в этом файле (отдельных файлов с данными нет):
//   - kGoodPrograms / kBadPrograms — программы с ожидаемым AST (S-expression) или ожидаемыми ошибками;
//   - функции E/C/P/Prog/Err/Ok ниже — маленькие проверки: приоритеты, ассоциативность, структура, позиции.
// Каждая проверка попадает в отчёт REPORT_HW2.md (имя, вид, вход, ожидание, результат); отчёт пишется
// в конце каждого запуска. Другой путь: FUNNY_REPORT=путь ./build/parser_tests (пустая строка — не писать).
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

#include "parser.hpp"

using namespace funnyparse;

#ifndef FUNNY_TOKENS
#define FUNNY_TOKENS "../lexer/data/tokens.txt"
#endif
static const char* kTokens = FUNNY_TOKENS;

// ---------- учёт результатов и отчёт ----------

struct Row {
    std::string kind, name, input, expected, got;  // kind: good | bad | robust
    bool pass;
};
static std::vector<Row> rows;
static int total = 0, failed = 0;

static void record(const std::string& kind, const std::string& name, const std::string& input,
                   const std::string& expected, const std::string& got) {
    bool pass = expected == got;
    ++total;
    if (!pass) {
        ++failed;
        std::cerr << "  FAIL [" << name << "]\n    input:    " << input << "\n    got:      " << got
                  << "\n    expected: " << expected << "\n";
    }
    rows.push_back({kind, name, input, expected, got, pass});
}
// Проверка без рекордера, для циклов и вспомогательных условий.
#define CHECK(cond) do { ++total; if (!(cond)) { ++failed; std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; } } while (0)

// Лексер строится один раз (ДКА из data/tokens.txt).
static const funnylex::Lexer& lexer() {
    static funnylex::Lexer l(funnylex::buildMinimalDFA(funnylex::loadTokenDefs(kTokens)));
    return l;
}
static ParseResult parse(const std::string& src) { return parseProgram(src, lexer()); }

// Ошибки в виде "строка:столбец: сообщение", по одной на строку.
static std::string errorsOf(const ParseResult& r) {
    std::string s;
    for (const auto& d : r.errors) s += std::to_string(d.line) + ":" + std::to_string(d.col) + ": " + d.message + "\n";
    return s;
}

// Разбирает программу с одним выражением/условием/предикатом внутри и возвращает S-expression нужной части.
static std::string exprOf(const std::string& e) {
    auto r = parse("f() returns r: int ensures true r = " + e + ";");
    return r.ok() ? dumpSexpr(*r.ast->find(Kind::Function)->find(Kind::Assign)->kids[0]) : "ERR " + errorsOf(r);
}
static std::string condOf(const std::string& c) {
    auto r = parse("f() returns r: int ensures true if (" + c + ") r = 1;");
    return r.ok() ? dumpSexpr(*r.ast->find(Kind::Function)->find(Kind::If)->kids[0]) : "ERR " + errorsOf(r);
}
static std::string predOf(const std::string& p) {
    auto r = parse("f() returns r: int ensures " + p + " r = 1;");
    return r.ok() ? dumpSexpr(*r.ast->find(Kind::Function)->find(Kind::Ensures)->kids[0]) : "ERR " + errorsOf(r);
}

static void E(const char* name, const std::string& e, const std::string& want) { record("good", name, e, want, exprOf(e)); }
static void C(const char* name, const std::string& c, const std::string& want) { record("good", name, c, want, condOf(c)); }
static void P(const char* name, const std::string& p, const std::string& want) { record("good", name, p, want, predOf(p)); }
// Программа целиком: ожидаемое AST (S-expression).
static void Prog(const char* name, const std::string& src, const std::string& want) {
    auto r = parse(src);
    record("good", name, src, want, r.ok() ? dumpSexpr(*r.ast) : "ERR " + errorsOf(r));
}
// Программа с ошибками: ожидаемый список "строка:столбец: сообщение".
static void Err(const char* name, const std::string& src, const std::string& want) {
    auto r = parse(src);
    record("bad", name, src, want, r.ok() ? "(no errors)" : errorsOf(r));
}
// Произвольная проверка: want и got — человекочитаемое описание ожидаемого и полученного.
static void Ok(const char* kind, const char* name, const std::string& input, const std::string& want, const std::string& got) {
    record(kind, name, input, want, got);
}
static const char* yesno(bool b) { return b ? "yes" : "no"; }

// ---------- таблицы программ (эталоны) ----------

struct ProgCase {
    const char* name;
    const char* src;
    const char* expected;
};

// Положительные программы: имя, исходник, ожидаемое AST (S-expression).
static const ProgCase kGoodPrograms[] = {
    {"arrays",
     R"FN(sum(a: int[]) returns s: int
  ensures s >= 0
  uses i: int
{
  s = 0;
  i = 0;
  while (i < length(a)) {
    s = s + a[i];
    i = i + 1;
  }
}

fill(a: int[], n: int) returns b: int[]
  ensures length(b) == length(a)
  uses i: int
{
  b = a;
  i = 0;
  while (i < n) invariant 0 <= i { b[i] = a[i + 1] * 2; i = i + 1; }
}
)FN",
     "(Program (Function sum (Params (VarDef a int[])) (Returns (VarDef s int)) (Ensures (>= s 0)) (Locals (VarDef i int)) (Block (Assign s 0) (Assign i 0) (While (< i (Call length a)) (Block (Assign s (+ s (Index a i))) (Assign i (+ i 1)))))) (Function fill (Params (VarDef a int[]) (VarDef n int)) (Returns (VarDef b int[])) (Ensures (== (Call length b) (Call length a))) (Locals (VarDef i int)) (Block (Assign b a) (Assign i 0) (While (< i n) (Invariant (<= 0 i)) (Block (ArrayAssign b i (* (Index a (+ i 1)) 2)) (Assign i (+ i 1)))))))"},
    {"comment_eof",
     R"FN(h() returns r: int ensures true r = 1; // comment at EOF, no trailing newline)FN",
     "(Program (Function h (Params) (Returns (VarDef r int)) (Ensures (BoolConst true)) (Assign r 1)))"},
    {"divide",
     R"FN(divide(a: int, b: int)
  requires b > 0
  returns q: int, r: int
  ensures a == q * b + r and 0 <= r and r < b
{
  q = 0;
  r = a;
  while (r >= b) { r = r - b; q = q + 1; }
}

// tuple assignment
useDivide(n: int) returns s: int
  ensures true
  uses q: int, r: int
{
  q, r = divide(n, 3);
  s = q + r;
}
)FN",
     "(Program (Function divide (Params (VarDef a int) (VarDef b int)) (Requires (> b 0)) (Returns (VarDef q int) (VarDef r int)) (Ensures (And (And (== a (+ (* q b) r)) (<= 0 r)) (< r b))) (Block (Assign q 0) (Assign r a) (While (>= r b) (Block (Assign r (- r b)) (Assign q (+ q 1)))))) (Function useDivide (Params (VarDef n int)) (Returns (VarDef s int)) (Ensures (BoolConst true)) (Locals (VarDef q int) (VarDef r int)) (Block (TupleAssign (Targets q r) (Call divide n 3)) (Assign s (+ q r)))))"},
    {"formulas",
     R"FN(sorted(a: int[], n: int) =>
  forall (i: int | i < 0 or i + 1 >= n or a[i] <= a[i + 1])

positive(x: int) => x > 0

hasZero(a: int[]) => exists (i: int | i >= 0 and i < length(a) and a[i] == 0)

first(a: int[]) requires sorted(a, length(a)) and positive(length(a))
  returns r: int
  ensures positive(r) and exists (k: int | k == r)
{
  r = a[0];
}
)FN",
     "(Program (Formula sorted (Params (VarDef a int[]) (VarDef n int)) (Forall i int (Or (Or (< i 0) (>= (+ i 1) n)) (<= (Index a i) (Index a (+ i 1)))))) (Formula positive (Params (VarDef x int)) (> x 0)) (Formula hasZero (Params (VarDef a int[])) (Exists i int (And (And (>= i 0) (< i (Call length a))) (== (Index a i) 0)))) (Function first (Params (VarDef a int[])) (Requires (And (FormulaRef sorted a (Call length a)) (FormulaRef positive (Call length a)))) (Returns (VarDef r int)) (Ensures (And (FormulaRef positive r) (Exists k int (== k r)))) (Block (Assign r (Index a 0)))))"},
    {"gcd",
     R"FN(// Euclid gcd: while with invariant and local variables
gcd(x: int, y: int)
  requires x > 0 and y > 0
  returns r: int
  ensures r > 0
  uses a: int, b: int
{
  a = x;
  b = y;
  while (a != b)
    invariant a > 0 and b > 0
  {
    if (a > b) a = a - b; else b = b - a;
  }
  r = a;
}
)FN",
     "(Program (Function gcd (Params (VarDef x int) (VarDef y int)) (Requires (And (> x 0) (> y 0))) (Returns (VarDef r int)) (Ensures (> r 0)) (Locals (VarDef a int) (VarDef b int)) (Block (Assign a x) (Assign b y) (While (!= a b) (Invariant (And (> a 0) (> b 0))) (Block (If (> a b) (Assign a (- a b)) (Assign b (- b a))))) (Assign r a))))"},
    {"implies_in_conditions",
     R"FN(g(x: int) returns r: int ensures true
{
  if (x > 0 → x > 1 → x > 2) r = x; else r = 0;
  if (x > 0 -> x > 1) r = 1;
}
)FN",
     "(Program (Function g (Params (VarDef x int)) (Returns (VarDef r int)) (Ensures (BoolConst true)) (Block (If (Implies (> x 0) (Implies (> x 1) (> x 2))) (Assign r x) (Assign r 0)) (If (Implies (> x 0) (> x 1)) (Assign r 1)))))"},
    {"minimal",
     R"FN(zero() returns r: int ensures true r = 0;
)FN",
     "(Program (Function zero (Params) (Returns (VarDef r int)) (Ensures (BoolConst true)) (Assign r 0)))"},
    {"precedence",
     R"FN(p(a: int, b: int, c: int) returns r: int
  ensures (a + b) * c > a - b - c and a / b / c <= -a * -b
{
  r = 1 + 2 * 3 - 4 / 2 + -a * (b - c);
  if ((a + b) > c and (a < b or not b < c) -> a == 0) r = 0;
}
)FN",
     "(Program (Function p (Params (VarDef a int) (VarDef b int) (VarDef c int)) (Returns (VarDef r int)) (Ensures (And (> (* (+ a b) c) (- (- a b) c)) (<= (/ (/ a b) c) (* (Neg a) (Neg b))))) (Block (Assign r (+ (- (+ 1 (* 2 3)) (/ 4 2)) (* (Neg a) (- b c)))) (If (Implies (And (> (+ a b) c) (Or (< a b) (Not (< b c)))) (== a 0)) (Assign r 0)))))"},
    {"statements",
     R"FN(f(x: int, y: int) returns z: int ensures true
{
  assume x >= 0;
  if (x > 0) if (y < 0) z = 1; else z = 5;
  if (x == 0) { z = 0; } else { z = -1; }
  { { z = z + 1; } }
  while (false) z = z;
  assert z != 100 or not (x < y);
}
)FN",
     "(Program (Function f (Params (VarDef x int) (VarDef y int)) (Returns (VarDef z int)) (Ensures (BoolConst true)) (Block (Assume (>= x 0)) (If (> x 0) (If (< y 0) (Assign z 1) (Assign z 5))) (If (== x 0) (Block (Assign z 0)) (Block (Assign z (Neg 1)))) (Block (Block (Assign z (+ z 1)))) (While (BoolConst false) (Assign z z)) (Assert (Or (!= z 100) (Not (< x y)))))))"},
};

// Отрицательные программы: имя, исходник, ожидаемые ошибки ("строка:столбец: сообщение", по одной на строку).
static const ProgCase kBadPrograms[] = {
    {"bad_condition",
     R"FN(f(x: int) returns r: int ensures true
{
  if (x) r = 1;
  while (x < ) r = 2;
}
)FN",
     "3:8: expected comparison operator (==, !=, <, <=, >, >=), found ')'\n4:14: expected expression, found ')'\n"},
    {"bad_type",
     R"FN(f(x: float) returns r: int ensures true r = 1;
)FN",
     "1:6: expected type 'int' or 'int[]', found 'float'\n"},
    {"chained_comparison",
     R"FN(f(x: int) returns r: int ensures x < 1 < 2 r = 1;
)FN",
     "1:40: unexpected '<' (comparisons cannot be chained)\n"},
    {"else_without_if",
     R"FN(f() returns r: int ensures true
{
  else r = 1;
}
)FN",
     "3:3: 'else' without matching 'if'\n"},
    {"empty",
     R"FN()FN",
     "1:1: empty program: expected at least one function or formula\n"},
    {"extra_brace",
     R"FN(f() returns r: int ensures true { r = 1; } }
)FN",
     "1:44: expected function or formula name, found '}'\n"},
    {"extra_number",
     R"FN(f() returns r: int ensures true r = 1; 42
)FN",
     "1:40: expected function or formula name, found '42'\n"},
    {"formula_semicolon",
     R"FN(positive(x: int) => x > 0;
)FN",
     "1:26: expected function or formula name, found ';'\n"},
    {"garbage_tokens",
     R"FN(f() returns r: int ensures true
{
  ) ) ) ;
  r = 1;
}
)FN",
     "3:3: expected statement, found ')'\n"},
    {"implication_in_predicate",
     R"FN(f(x: int) returns r: int
  ensures x > 0 -> r > 0
  r = x;
)FN",
     "2:17: implication is allowed only in conditions, not in predicates, found '->'\n"},
    {"lexical_errors",
     R"FN(f(my_var: int) returns r: int ensures true r = 1 @ 2;
)FN",
     "1:5: unexpected character '_' (identifiers may contain only letters and digits)\n1:6: expected ':' after variable name, found 'var'\n1:50: unexpected character '@'\n"},
    {"missing_returns",
     R"FN(f(x: int) ensures true { x = 1; }
)FN",
     "1:11: expected 'returns' in function header, found 'ensures'\n"},
    {"missing_semicolon",
     R"FN(f(x: int) returns r: int ensures true
{
  r = x
  r = r + 1;
}
)FN",
     "4:3: expected ';' after assignment, found 'r'\n"},
    {"only_comment",
     R"FN(  
 // only a comment
)FN",
     "3:1: empty program: expected at least one function or formula\n"},
    {"quantifier_in_condition",
     R"FN(f(x: int) returns r: int ensures true
{
  if (forall (i: int | i > 0)) r = 1;
}
)FN",
     "3:7: quantifiers are allowed only in predicates, not in conditions\n"},
    {"recovery_next_def",
     R"FN(f() returns r: int
  ensures true
{ r = 1; }
g(x: int) returns
{ }
h() returns r: int ensures true r = 2;
)FN",
     "5:1: expected identifier as variable name, found '{'\n"},
    {"several_errors",
     R"FN(f(x: int) returns r: int ensures true
{
  r = ;
  r = 1 + ;
  r = 2;
  r = * 3;
}
)FN",
     "3:7: expected expression, found ';'\n4:11: expected expression, found ';'\n6:7: expected expression, found '*'\n"},
    {"tuple_needs_call",
     R"FN(f(x: int) returns a: int, b: int ensures true
{
  a, b = x + 1;
}
)FN",
     "3:12: tuple assignment requires a function call on the right side\n"},
    {"unclosed_brace",
     R"FN(f(x: int) returns r: int ensures true
{
  r = x;
)FN",
     "4:1: expected '}' to close '{' opened at 2:1, found end of input\n"},
    {"unclosed_paren_expr",
     R"FN(f(x: int) returns r: int ensures true
{
  r = (x + 1;
}
)FN",
     "3:13: expected ')' to close '(' opened at 3:7, found ';'\n"},
    {"unclosed_paren_params",
     R"FN(f(x: int returns r: int
  ensures true
  r = x;
)FN",
     "1:10: expected ')' after parameters, found 'returns'\n"},
    {"wrong_operator",
     R"FN(f(x: int) returns r: int ensures true
{
  if (x = 1) r = 0;
  r == 1;
}
)FN",
     "3:9: expected comparison operator (==, !=, <, <=, >, >=), found '=' (did you mean '=='?)\n4:5: expected '=' in assignment, found '=='\n"},
};


// ---------- арифметика ----------

static void test_arith() {
    E("arith_mul_over_add", "1 + 2 * 3", "(+ 1 (* 2 3))");
    E("arith_mul_then_add", "1 * 2 + 3", "(+ (* 1 2) 3)");
    E("arith_parens", "(1 + 2) * 3", "(* (+ 1 2) 3)");
    E("arith_div_over_sub", "a - b / c", "(- a (/ b c))");
    E("arith_sub_left_assoc", "a - b - c", "(- (- a b) c)");
    E("arith_div_left_assoc", "a / b / c", "(/ (/ a b) c)");
    E("arith_sub_add_left_assoc", "a - b + c", "(+ (- a b) c)");
    E("arith_mul_div_left_assoc", "a * b / c * d", "(* (/ (* a b) c) d)");
    E("unary_minus_over_mul", "-a * b", "(* (Neg a) b)");
    E("unary_minus_rhs", "a * -b", "(* a (Neg b))");
    E("unary_minus_double", "- - a", "(Neg (Neg a))");
    E("unary_minus_after_binary", "a - -b", "(- a (Neg b))");
    E("call_args", "f(1, g(2), x + 1)", "(Call f 1 (Call g 2) (+ x 1))");
    E("call_no_args", "f()", "(Call f)");
    E("array_read", "a[i + 1] * 2", "(* (Index a (+ i 1)) 2)");
    E("array_nested_index", "a[b[0]]", "(Index a (Index b 0))");
    E("builtin_length", "length(a) - 1", "(- (Call length a) 1)");
}

// ---------- условия и предикаты ----------

static void test_conditions() {
    C("cond_and_over_or_1", "a > 0 and b > 0 or c > 0", "(Or (And (> a 0) (> b 0)) (> c 0))");
    C("cond_and_over_or_2", "a > 0 or b > 0 and c > 0", "(Or (> a 0) (And (> b 0) (> c 0)))");
    C("cond_not_over_and", "not a > 0 and b > 0", "(And (Not (> a 0)) (> b 0))");
    C("cond_not_not", "not not true", "(Not (Not (BoolConst true)))");
    C("cond_and_left_assoc", "a > 0 and b > 0 and c > 0", "(And (And (> a 0) (> b 0)) (> c 0))");
    C("cond_or_left_assoc", "a > 0 or b > 0 or c > 0", "(Or (Or (> a 0) (> b 0)) (> c 0))");
    C("cond_implies_right_assoc", "a > 0 -> b > 0 -> c > 0", "(Implies (> a 0) (Implies (> b 0) (> c 0)))");
    C("cond_implies_lowest", "a > 0 or b > 0 -> c > 0", "(Implies (Or (> a 0) (> b 0)) (> c 0))");
    C("cond_implies_unicode_arrow", "a > 0 \xE2\x86\x92 b > 0", "(Implies (> a 0) (> b 0))");
    for (const char* op : {"==", "!=", "<=", ">=", "<", ">"})
        C((std::string("cond_compare_") + op).c_str(), std::string("a ") + op + " b", std::string("(") + op + " a b)");
    C("cond_paren_arith", "(a + b) > c", "(> (+ a b) c)");
    C("cond_paren_bool", "(a > 0) and (b > 0)", "(And (> a 0) (> b 0))");
    C("cond_paren_double", "((a > 0))", "(> a 0)");
    C("cond_paren_or_then_and", "(a > 0 or b > 0) and c > 0", "(And (Or (> a 0) (> b 0)) (> c 0))");
    C("cond_paren_arith_both_sides", "(a) - 1 > (b)", "(> (- a 1) b)");
    C("cond_not_paren", "not (a > 0)", "(Not (> a 0))");
}
static void test_predicates() {
    P("pred_forall", "forall (i: int | i > 0)", "(Forall i int (> i 0))");
    P("pred_exists_array", "exists (a: int[] | length(a) == 0)", "(Exists a int[] (== (Call length a) 0))");
    P("pred_formula_ref", "p(x, 1)", "(FormulaRef p x 1)");
    P("pred_formula_ref_and_call", "p(x) and f(x) > 0", "(And (FormulaRef p x) (> (Call f x) 0))");
    P("pred_not_formula_ref", "not p(x) and q()", "(And (Not (FormulaRef p x)) (FormulaRef q))");
    P("pred_or_and_priority", "a > 0 or b > 0 and c > 0", "(Or (> a 0) (And (> b 0) (> c 0)))");
    P("pred_nested_quantifiers", "forall (i: int | exists (j: int | j > i)) and true",
      "(And (Forall i int (Exists j int (> j i))) (BoolConst true))");
}

// ---------- структура программы ----------

static void test_structure() {
    {
        std::string src = "f(x: int, y: int) returns z: int ensures true if (x > 0) if (y < 0) z = 1; else z = 5;";
        auto r = parse(src);
        const Node* outer = r.ok() ? r.ast->find(Kind::Function)->find(Kind::If) : nullptr;
        std::string got = outer ? "outer if kids=" + std::to_string(outer->kids.size()) + ", inner if kids=" +
                                      std::to_string(outer->kids[1]->kids.size())
                                : "ERR " + errorsOf(r);
        Ok("good", "dangling_else_binds_to_nearest_if", src, "outer if kids=2, inner if kids=3", got);
    }
    {
        std::string src = "f(a: int[], n: int) returns r: int, b: int[] uses i: int, j: int { r = 0; }";
        auto r = parse(src);
        const Node* f = r.ok() ? r.ast->find(Kind::Function) : nullptr;
        std::string got = "ERR";
        if (f)
            got = "name=" + f->text + " requires=" + yesno(f->find(Kind::Requires)) + " ensures=" + yesno(f->find(Kind::Ensures)) +
                  " params=" + std::to_string(f->find(Kind::Params)->kids.size()) + " param0=" + f->find(Kind::Params)->kids[0]->extra +
                  " returns=" + std::to_string(f->find(Kind::Returns)->kids.size()) +
                  " locals=" + std::to_string(f->find(Kind::Locals)->kids.size()) + " body=" + yesno(f->find(Kind::Block));
        Ok("good", "function_structure_optional_parts", src,
           "name=f requires=no ensures=no params=2 param0=int[] returns=2 locals=2 body=yes", got);
    }
    Prog("formulas_without_semicolon", "pos(x: int) => x > 0  neg(x: int) => x < 0 same() => true",
         "(Program (Formula pos (Params (VarDef x int)) (> x 0)) (Formula neg (Params (VarDef x int)) (< x 0)) (Formula same (Params) (BoolConst true)))");
    Prog("all_statement_kinds",
         "f(a: int[]) returns x: int, y: int ensures true "
         "{ a2 = 1; a[x + 1] = 2; x, y = g(1); assert x > 0; assume y > 0; while (x > 0) invariant x >= 0 x = x - 1; }",
         "(Program (Function f (Params (VarDef a int[])) (Returns (VarDef x int) (VarDef y int)) (Ensures (BoolConst true)) "
         "(Block (Assign a2 1) (ArrayAssign a (+ x 1) 2) (TupleAssign (Targets x y) (Call g 1)) (Assert (> x 0)) (Assume (> y 0)) "
         "(While (> x 0) (Invariant (>= x 0)) (Assign x (- x 1))))))");
    Prog("single_assign_from_call_is_plain_assign", "f() returns r: int ensures true r = g(1);",
         "(Program (Function f (Params) (Returns (VarDef r int)) (Ensures (BoolConst true)) (Assign r (Call g 1))))");
    Prog("else_if_chain", "f() returns r: int ensures true if (true) r = 1; else if (false) r = 2; else r = 3;",
         "(Program (Function f (Params) (Returns (VarDef r int)) (Ensures (BoolConst true)) "
         "(If (BoolConst true) (Assign r 1) (If (BoolConst false) (Assign r 2) (Assign r 3)))))");
    Prog("empty_block", "f() returns r: int ensures true { }",
         "(Program (Function f (Params) (Returns (VarDef r int)) (Ensures (BoolConst true)) (Block)))");
}

// ---------- формат вывода и позиции ----------

static void test_output_and_positions() {
    {
        std::string src = "f() returns r: int ensures true\n{\n  r = 1 +\n      2;\n}\n";
        auto r = parse(src);
        std::string got = "ERR";
        if (r.ok()) {
            const Node* f = r.ast->find(Kind::Function);
            const Node* as = f->find(Kind::Block)->kids[0].get();
            auto p = [](const Node* n) { return std::to_string(n->line) + ":" + std::to_string(n->col); };
            got = "function " + p(f) + ", r " + p(as) + ", + " + p(as->kids[0].get()) + ", 2 " + p(as->kids[0]->kids[1].get());
        }
        Ok("good", "node_positions_multiline", src, "function 1:1, r 3:3, + 3:9, 2 4:7", got);
    }
    Err("crlf_error_position", "f() returns r: int ensures true\r\n{\r\n  r = ;\r\n}\r\n", "3:7: expected expression, found ';'\n");
    Err("lone_cr_error_position", "f() returns r: int ensures true\r{\r  r = ;\r}\r", "3:7: expected expression, found ';'\n");
    {
        std::string src = "zero() returns r: int ensures true r = 0;";
        auto r = parse(src);
        std::string j = dumpJSON(*r.ast);
        bool ok = j.find("\"kind\": \"Program\"") != std::string::npos &&
                  j.find("\"kind\": \"Number\", \"text\": \"0\", \"line\": 1, \"col\": 40") != std::string::npos &&
                  j.find("\"type\": \"int\"") != std::string::npos;
        Ok("good", "json_output_has_kind_position_type", src, "Program, Number 0 at 1:40, type int present", ok ? "Program, Number 0 at 1:40, type int present" : "missing fields");
    }
    {
        std::string src = "f() returns r: int ensures true r = a[i] + 1;";
        auto r = parse(src);
        std::string got = r.ok() ? dumpTree(*r.ast->find(Kind::Function)->find(Kind::Assign)) : "ERR " + errorsOf(r);
        Ok("good", "tree_output_format", src, "Assign r / Binary + / Index a / Var i / Number 1 (2 spaces per level)",
           got == "Assign r\n  Binary +\n    Index a\n      Var i\n    Number 1\n"
               ? "Assign r / Binary + / Index a / Var i / Number 1 (2 spaces per level)" : got);
    }
    {
        std::string src = "f() returns r: int ensures true r = ;\n";
        auto r = parse(src);
        std::string got = r.errors.empty() ? "" : formatDiagnostic(src, r.errors[0], "a.fn");
        std::string want = "a.fn:1:37: error: expected expression, found ';'\n    f() returns r: int ensures true r = ;\n"
                           "                                        ^\n";
        Ok("bad", "format_diagnostic_with_caret", src, "a.fn:1:37: error: expected expression, found ';' + source line + caret under col 37",
           got == want ? "a.fn:1:37: error: expected expression, found ';' + source line + caret under col 37" : got);
    }
}

// ---------- ошибки (маленькие программы) ----------

static void test_errors() {
    Err("empty_input", "", "1:1: empty program: expected at least one function or formula\n");
    Err("only_whitespace_and_comment", "  \n// nothing\n", "3:1: empty program: expected at least one function or formula\n");
    Err("unclosed_paren_params", "f(x: int returns r: int ensures true r = x;", "1:10: expected ')' after parameters, found 'returns'\n");
    Err("unclosed_paren_expr", "f() returns r: int ensures true r = (1 + 2;", "1:43: expected ')' to close '(' opened at 1:37, found ';'\n");
    Err("unclosed_brace", "f() returns r: int ensures true { r = 1;", "1:41: expected '}' to close '{' opened at 1:33, found end of input\n");
    Err("extra_closing_brace", "f() returns r: int ensures true r = 1; }", "1:40: expected function or formula name, found '}'\n");
    Err("extra_number_in_statement", "f() returns r: int ensures true r = 1 2;", "1:39: expected ';' after assignment, found '2'\n");
    Err("assign_instead_of_eq_in_condition", "f(x: int) returns r: int ensures true if (x = 1) r = 0;",
        "1:45: expected comparison operator (==, !=, <, <=, >, >=), found '=' (did you mean '=='?)\n");
    Err("wrong_operator_eq_lt", "f() returns r: int ensures true r =< 1;", "1:36: expected expression, found '<'\n");
    Err("expression_statement", "f() returns r: int ensures true r + 1;", "1:35: expected '=' in assignment, found '+'\n");
    Err("unclosed_bracket_index", "f() returns r: int ensures true r = a[1;", "1:40: expected ']' to close '[' opened at 1:38, found ';'\n");
    Err("unclosed_paren_call", "f() returns r: int ensures true r = g(1, 2;", "1:43: expected ')' to close '(' opened at 1:38, found ';'\n");
    Err("if_without_parens", "f() returns r: int ensures true if r > 0 r = 1;", "1:36: expected '(' after 'if', found 'r'\n");
    Err("while_without_parens", "f() returns r: int ensures true while x > 0 r = 1;", "1:39: expected '(' after 'while', found 'x'\n");
    Err("vardef_without_colon", "f(x int) returns r: int ensures true r = 1;", "1:5: expected ':' after variable name, found 'int'\n");
    Err("assign_to_literal", "f() returns r: int ensures true 1 = r;", "1:33: expected statement, found '1'\n");
    Err("missing_body", "f() returns r: int ensures true", "1:32: expected statement, found end of input\n");
    Err("quantifier_without_pipe", "f() returns r: int ensures forall (i: int i > 0) r = 1;", "1:43: expected '|' after quantifier variable, found 'i'\n");
    Err("double_operator", "f() returns r: int ensures true r = 1 + * 2;", "1:41: expected expression, found '*'\n");
    Err("missing_final_semicolon", "f() returns r: int ensures true r = 1 + 2", "1:42: expected ';' after assignment, found end of input\n");
    Err("assert_without_predicate", "f() returns r: int ensures true { assert ; }", "1:42: expected predicate, found ';'\n");
    Err("definition_without_name", "() returns r: int ensures true r = 1;", "1:1: expected function or formula name, found '('\n");
    Err("empty_array_index", "f() returns r: int ensures true r = a[];", "1:39: expected expression, found ']'\n");
    Err("dangling_else_at_eof", "f() returns r: int ensures true if (x > 0) r = 1; else", "1:55: expected statement, found end of input\n");
    Err("two_dimensional_index", "f() returns r: int ensures true r = a[1][2];", "1:41: expected ';' after assignment, found '['\n");
}
static void test_deviations() {
    // импликация только в condition: в predicate — ошибка (и "->", и "→")
    Err("implication_in_ensures", "f() returns r: int ensures a > 0 -> r > 0 r = 1;",
        "1:34: implication is allowed only in conditions, not in predicates, found '->'\n");
    Err("unicode_implication_in_ensures", "f() returns r: int ensures a > 0 \xE2\x86\x92 r > 0 r = 1;",
        "1:34: implication is allowed only in conditions, not in predicates, found '\xE2\x86\x92'\n");
    Err("implication_in_requires", "f() requires a > 0 -> r > 0 returns r: int ensures true r = 1;",
        "1:20: implication is allowed only in conditions, not in predicates, found '->'\n");
    Err("implication_in_formula", "p(x: int) => x > 0 -> x > 1", "1:20: implication is allowed only in conditions, not in predicates, found '->'\n");
    Err("implication_in_quantifier_body", "f() returns r: int ensures forall (i: int | i > 0 -> i > 1) r = 1;",
        "1:51: implication is allowed only in conditions, not in predicates, found '->'\n");
    Err("implication_in_parenthesised_predicate", "f() returns r: int ensures (a > 0 -> r > 0) r = 1;",
        "1:35: expected ')' to close '(' opened at 1:28, found '->'\n");
    // ';' после формулы не токен формулы: на верхнем уровне это ошибка
    Err("semicolon_after_formula", "p(x: int) => x > 0;", "1:19: expected function or formula name, found ';'\n");
    Err("lexical_error_at_sign", "f() returns r: int ensures true r = 1 @ 2;",
        "1:39: unexpected character '@'\n1:41: expected ';' after assignment, found '2'\n");
    Err("underscore_in_identifier", "f(a_b: int) returns r: int ensures true r = 1;",
        "1:4: unexpected character '_' (identifiers may contain only letters and digits)\n1:5: expected ':' after variable name, found 'b'\n");
}
static void test_recovery() {
    // три плохих оператора: должны быть найдены все три, хорошие остаются в AST
    std::string src = "f() returns r: int ensures true {\n r = ;\n r = 2;\n r = 1 + ;\n r = 3;\n r = * 3;\n}\n";
    auto r = parse(src);
    std::string got = std::to_string(r.errors.size()) + " errors at lines";
    for (auto& e : r.errors) got += " " + std::to_string(e.line);
    const Node* b = r.ast->find(Kind::Function) ? r.ast->find(Kind::Function)->find(Kind::Block) : nullptr;
    got += "; statements kept=" + std::to_string(b ? b->kids.size() : 0);
    Ok("bad", "recovery_inside_block", src, "3 errors at lines 2 4 6; statements kept=2", got);
    // после битой функции следующая разбирается нормально
    std::string src2 = "f() returns\n{ }\ng() returns r: int ensures true r = 1;";
    auto r2 = parse(src2);
    std::string got2 = std::to_string(r2.errors.size()) + " error; definitions kept=" + std::to_string(r2.ast->kids.size()) +
                       (r2.ast->kids.empty() ? "" : " (" + r2.ast->kids[0]->text + ")");
    Ok("bad", "recovery_next_definition", src2, "1 error; definitions kept=1 (g)", got2);
}

// ---------- стык с лексером HW1 ----------

// Типы токенов, которые парсер ждёт от лексера (имена из data/tokens.txt).
static const char* const kNeededTokens[] = {
    "IDENT", "INT", "KW_RETURNS", "KW_REQUIRES", "KW_ENSURES", "KW_USES", "KW_IF", "KW_ELSE", "KW_WHILE", "KW_INVARIANT",
    "KW_TRUE", "KW_FALSE", "KW_NOT", "KW_AND", "KW_OR", "KW_FORALL", "KW_EXISTS", "KW_LENGTH", "KW_ASSERT", "KW_ASSUME",
    "KW_INT", "OP_ARROW", "OP_IMPLIES", "OP_EQ", "OP_NEQ", "OP_LE", "OP_GE", "OP_ASSIGN", "OP_LT", "OP_GT", "OP_LPAREN",
    "OP_RPAREN", "OP_LBRACKET", "OP_RBRACKET", "OP_LBRACE", "OP_RBRACE", "OP_COMMA", "OP_SEMI", "OP_COLON", "OP_PLUS",
    "OP_MINUS", "OP_STAR", "OP_SLASH", "OP_PIPE"};

static void test_lexer_integration() {
    // контракт: все нужные парсеру типы токенов есть в таблице лексера (переименование в tokens.txt ломает тест)
    {
        std::string missing;
        const auto& names = lexer().dfa().names;
        for (const char* need : kNeededTokens)
            if (std::find(names.begin(), names.end(), need) == names.end()) missing += std::string(need) + " ";
        Ok("good", "lexer_contract_token_names", "token names the parser relies on (" + std::to_string(sizeof kNeededTokens / sizeof *kNeededTokens) + ") vs DFA names",
           "all present", missing.empty() ? "all present" : "missing: " + missing);
    }
    // позиции токенов лексера доходят до AST и диагностик (столбец считается в байтах, таб = 1)
    Err("tab_counts_as_one_column", "f()\treturns r: int ensures true r = ;", "1:37: expected expression, found ';'\n");
    Err("column_counts_bytes_not_chars", "f() returns r: int ensures true r = \xC3\xA9;",
        "1:37: unexpected characters '\xC3\xA9'\n1:39: expected expression, found ';'\n");
    // границы токенов, которые определяет ДКА (максимальное совпадение)
    Err("int_00_is_two_tokens", "f() returns r: int ensures true r = 00;", "1:38: expected ';' after assignment, found '0'\n");
    Err("int_then_letters", "f() returns r: int ensures true r = 1a;", "1:38: expected ';' after assignment, found 'a'\n");
    Err("arrow_is_not_assign", "f() returns r: int ensures true r => 1;", "1:35: expected '=' in assignment, found '=>'\n");
    Err("triple_eq_is_eq_then_assign", "f() returns r: int ensures true r === 1;", "1:35: expected '=' in assignment, found '=='\n");
    Err("underscore_splits_identifier", "f() returns r: int ensures true r = a_b;",
        "1:38: unexpected character '_' (identifiers may contain only letters and digits)\n1:39: expected ';' after assignment, found 'b'\n");
    // ключевые слова лексера
    Err("keyword_function_is_not_a_header", "function f() returns r: int ensures true r = 1;",
        "1:1: expected function or formula name, found 'function' ('function' is a reserved word)\n");
    Err("keyword_as_variable_name", "f() returns length: int ensures true length = 1;",
        "1:13: expected identifier as variable name, found 'length' ('length' is a reserved word)\n");
    Prog("keyword_prefix_is_identifier", "iffy() returns functions: int ensures true functions = 1;",
         "(Program (Function iffy (Params) (Returns (VarDef functions int)) (Ensures (BoolConst true)) (Assign functions 1)))");
    Prog("formula_is_not_keyword", "formula() => true",
         "(Program (Formula formula (Params) (BoolConst true)))");
    // лексер HW1 знает только ASCII: хвост комментария с не-ASCII парсер возвращает в комментарий
    const std::string fnR1 = "(Program (Function f (Params) (Returns (VarDef r int)) (Ensures (BoolConst true)) (Assign r 1)))";
    Prog("comment_cyrillic_own_line", "// \xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82, \xD0\xBC\xD0\xB8\xD1\x80\nf() returns r: int ensures true r = 1;", fnR1);
    Prog("comment_cyrillic_after_code",
         "f() returns r: int ensures true\n{\n  r = 1; // \xD1\x81\xD1\x87\xD0\xB8\xD1\x82\xD0\xB0\xD0\xB5\xD0\xBC: x = 1\n}\n",
         "(Program (Function f (Params) (Returns (VarDef r int)) (Ensures (BoolConst true)) (Block (Assign r 1))))");
    Prog("comment_ascii_words_after_non_ascii", "f() returns r: int ensures true r = 1; // na\xC3\xAFve code ) ( { ;\n", fnR1);
    Prog("comment_arrow_is_not_implication", "f() returns r: int ensures true r = 1; // a \xE2\x86\x92 b -> c\n", fnR1);
    Prog("comment_non_ascii_lone_cr", "// \xD0\xB0\rf() returns r: int ensures true r = 1;", fnR1);
    Prog("comment_non_ascii_at_eof", "f() returns r: int ensures true r = 1; //\xD0\xB1", fnR1);
    Prog("comment_non_ascii_on_every_line",
         "// \xD0\xB0\nf() returns // \xD0\xB1\nr: int ensures true // \xD0\xB2\n r = 1; // \xD0\xB3\n", fnR1);
    // а вот вне комментария не-ASCII остаётся ошибкой
    Err("non_ascii_after_single_slash_is_error", "f() returns r: int ensures true r = 1; /\xC3\xA9",
        "1:40: expected function or formula name, found '/'\n1:41: unexpected characters '\xC3\xA9'\n");
    Err("non_ascii_in_code_then_comment", "f() returns r: int ensures true r = \xC3\xA9; // \xC3\xA9\n",
        "1:37: unexpected characters '\xC3\xA9'\n1:39: expected expression, found ';'\n");
    Err("non_ascii_on_next_line_after_comment", "f() returns r: int ensures true // c\nr = \xC3\xA9;",
        "2:5: unexpected characters '\xC3\xA9'\n2:7: expected expression, found ';'\n");
}

// ---------- программы из таблиц ----------

static void test_program_tables() {
    for (const auto& c : kGoodPrograms) Prog(c.name, c.src, c.expected);
    for (const auto& c : kBadPrograms) Err(c.name, c.src, c.expected);
}

// ---------- отсутствие зацикливания ----------

static unsigned rngState = 12345;
static unsigned rnd(unsigned n) { rngState = rngState * 1664525u + 1013904223u; return (rngState >> 8) % n; }

// Все программы из таблицы good, склеенные в список.
static std::vector<std::string> goodSources() {
    std::vector<std::string> v;
    for (const auto& c : kGoodPrograms) v.push_back(c.src);
    return v;
}

static void test_robustness() {
    const std::string terminates = "terminates, no crash";
    {
        int runs = 0;
        for (const auto& src : goodSources())
            for (size_t n = 0; n <= src.size(); ++n) { parse(src.substr(0, n)); ++runs; }
        CHECK(runs > 1000);
        Ok("robust", "every_prefix_of_good_programs", "all prefixes of the good programs (" + std::to_string(runs) + " runs)", terminates, terminates);
    }
    {
        const std::string junk = " (){};,=<>+-*/|:[]a1\n!@_";
        auto progs = goodSources();
        for (int it = 0; it < 4000; ++it) {
            std::string s = progs[rnd(progs.size())];
            for (int k = 0, m = 1 + rnd(4); k < m; ++k) {
                size_t i = rnd(s.size());
                unsigned op = rnd(3);
                if (op == 0) s[i] = junk[rnd(junk.size())];
                else if (op == 1) s.erase(i, 1 + rnd(5));
                else s.insert(i, 1, junk[rnd(junk.size())]);
            }
            parse(s);
        }
        Ok("robust", "random_mutations_of_good_programs", "4000 good programs with 1-4 random edits", terminates, terminates);
    }
    {
        const char* pool[] = {"f", "x", "1", "(", ")", "{", "}", "[", "]", ",", ";", ":", "|", "=", "==", "<", "+", "-", "*", "/",
                              "=>", "->", "if", "else", "while", "invariant", "returns", "requires", "ensures", "uses", "int",
                              "and", "or", "not", "forall", "exists", "true", "false", "length", "assert", "assume"};
        const unsigned n = sizeof pool / sizeof *pool;
        for (int it = 0; it < 6000; ++it) {
            std::string s;
            for (int k = 0, len = rnd(40); k < len; ++k) s += std::string(pool[rnd(n)]) + " ";
            parse(s);
        }
        Ok("robust", "random_token_sequences", "6000 random sequences of up to 40 tokens", terminates, terminates);
    }
    {
        auto r = parse("f() returns r: int ensures true r = " + std::string(5000, '(') + "1" + std::string(5000, ')') + ";");
        auto r2 = parse("f() returns r: int ensures true " + std::string(5000, '{'));
        auto r3 = parse("f() returns r: int ensures " + std::string(5000, '-') + "1 > 0 r = 1;");
        Ok("robust", "deep_nesting_5000_is_error_not_crash", "5000 x '(' / '{' / '-'", "3 errors reported, no stack overflow",
           std::to_string(!r.ok() + !r2.ok() + !r3.ok()) + " errors reported, no stack overflow");
        auto r4 = parse("f() returns r: int ensures true r = " + std::string(150, '(') + "1" + std::string(150, ')') + ";");
        Ok("robust", "moderate_nesting_150_ok", "150 x '(' around 1", "parses without errors", r4.ok() ? "parses without errors" : errorsOf(r4));
    }
}

// ---------- отчёт ----------

// Длина корректной UTF-8 последовательности (2-4 байта) в позиции i, иначе 1.
static size_t utf8Len(const std::string& s, size_t i) {
    unsigned char c = s[i];
    size_t n = c >= 0xF0 && c < 0xF8 ? 4 : c >= 0xE0 ? (c < 0xF0 ? 3 : 1) : c >= 0xC2 && c < 0xE0 ? 2 : 1;
    if (n == 1 || i + n > s.size()) return 1;
    for (size_t k = 1; k < n; ++k)
        if ((static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80) return 1;
    return n;
}
// Вход/ожидание в одну строку таблицы: экранируем служебные символы, режем длинное.
static std::string cell(const std::string& s, size_t limit = 240) {
    std::string o;
    size_t shown = 0;
    for (size_t i = 0; i < s.size(); ++shown) {
        if (shown >= limit) { o += "…(" + std::to_string(s.size()) + " chars)"; break; }
        unsigned char c = s[i];
        size_t n = utf8Len(s, i);  // корректная многобайтная последовательность печатается как есть
        if (n > 1) { o += s.substr(i, n); i += n; continue; }
        ++i;
        if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else if (c == '\t') o += "\\t";
        else if (c == '|') o += "\\|";
        else if (c < 32 || c >= 127) { char b[8]; snprintf(b, sizeof b, "\\x%02x", c); o += b; }
        else o += static_cast<char>(c);
    }
    return o.empty() ? "" : "`" + o + "`";
}
// Ожидаемые ошибки: по одной в строке ячейки.
static std::string cellErrors(const std::string& s) {
    std::string out, line;
    std::istringstream in(s);
    bool first = true;
    while (std::getline(in, line)) { out += (first ? "" : "<br>") + cell(line); first = false; }
    return out.empty() ? cell(s) : out;
}

static void writeReport(const std::string& path) {
    int good = 0, bad = 0, robust = 0;
    for (auto& r : rows) (r.kind == "good" ? good : r.kind == "bad" ? bad : robust)++;
    int passed = 0;
    for (auto& r : rows) passed += r.pass;
    std::ofstream o(path, std::ios::binary);
    o << "# HW2 report\n\n";
    o << "Parser: LL(1), recursive descent; tokens come from the HW1 lexer (DFA built from `../lexer/data/tokens.txt`), "
         "every token carries line:column. Errors are collected in panic mode.\n\n";
    o << "## Tests: " << passed << "/" << rows.size() << " passed (good " << good << ", bad " << bad << ", robust " << robust << ")\n\n";
    o << "`good` — program/fragment must parse and give the expected AST (S-expression); "
         "`bad` — must give exactly the expected diagnostics (`line:col: message`); "
         "`robust` — parser must terminate without crashing on broken input.\n\n";
    o << "| name | kind | input | expected | result |\n|---|---|---|---|---|\n";
    for (auto& r : rows) {
        o << "| " << r.name << " | " << r.kind << " | " << cell(r.input) << " | "
          << (r.kind == "bad" && r.expected.find(':') != std::string::npos && r.expected.back() == '\n' ? cellErrors(r.expected) : cell(r.expected, 400))
          << " | " << (r.pass ? "PASS" : "**FAIL**") << " |\n";
    }
    if (failed) {
        o << "\n## Failures\n\n";
        for (auto& r : rows)
            if (!r.pass) o << "- **" << r.name << "**: got " << cell(r.got, 400) << "\n";
    }
    o << "\n## Как устроены тесты `robust`\n\n"
         "Эти тесты не сверяют результат с эталоном: они проверяют, что парсер не падает и не зависает "
         "(`parse(...)` возвращает управление) на испорченном вводе. Один «запуск» — один вызов `parse(...)` на одном тексте. "
         "Случайные тесты используют генератор с фиксированным начальным значением, поэтому каждый прогон одинаков.\n\n"
         "- **Все префиксы хороших программ** (число запусков указано в таблице). Каждая хорошая программа режется на куски: "
         "первые 0 символов, первый 1, первые 2, и так до полной длины. Это имитирует обрыв ввода в любом месте: "
         "незакрытые скобки, обрыв посреди ключевого слова и т. д.\n"
         "- **Случайные мутации** (4000 запусков). Берётся случайная хорошая программа, к ней применяется от 1 до 4 случайных правок: "
         "заменить символ, удалить от 1 до 5 символов или вставить символ. Символы берутся из набора мусора "
         "`(){};,=<>+-*/|:[]a1`, перевод строки, `!@_`.\n"
         "- **Случайные последовательности токенов** (6000 запусков). Из пула слов (`f`, `x`, `1`, `(`, `if`, `while`, `returns`, "
         "`forall` и т. д.) склеивается до 40 случайных слов через пробел: получается абракадабра, но из корректных токенов.\n"
         "- **Глубокая вложенность.** 5000 открывающих скобок `(`, `{` или минусов `-` подряд. Парсер должен вернуть ошибку, "
         "а не упасть от переполнения стека. Отдельно проверяется, что 150 скобок вокруг единицы разбираются без ошибок.\n";
}

int main() {
    struct { const char* name; void (*fn)(); } all[] = {
        {"arithmetic: precedence, associativity, calls, arrays", test_arith},
        {"conditions: precedence, implication, parentheses", test_conditions},
        {"predicates: quantifiers, formula references", test_predicates},
        {"program structure: functions, formulas, statements", test_structure},
        {"output formats and node positions", test_output_and_positions},
        {"errors: messages and positions", test_errors},
        {"deviations from the description and lexical errors", test_deviations},
        {"error recovery", test_recovery},
        {"integration with the HW1 lexer", test_lexer_integration},
        {"program tables (good/bad)", test_program_tables},
        {"robustness: termination", test_robustness},
    };
    for (auto& t : all) {
        int before = failed;
        try { t.fn(); } catch (const std::exception& ex) { ++failed; std::cerr << "  EXCEPTION: " << ex.what() << "\n"; }
        std::cout << (failed == before ? "[ OK ] " : "[FAIL] ") << t.name << "\n";
    }
    const char* rp = std::getenv("FUNNY_REPORT");
    std::string path = rp ? rp : "REPORT_HW2.md";
    if (!path.empty()) writeReport(path);
    std::cout << (total - failed) << "/" << total << " checks passed";
    if (!path.empty()) std::cout << "; report: " << path;
    std::cout << "\n";
    return failed ? 1 : 0;
}
