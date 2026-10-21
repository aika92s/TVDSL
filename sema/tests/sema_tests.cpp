
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

#include "sema.hpp"

using namespace funnyparse;
using namespace funnysema;

#ifndef FUNNY_TOKENS
#define FUNNY_TOKENS "../lexer/data/tokens.txt"
#endif
static const char* kTokens = FUNNY_TOKENS;

//  учёт результатов и отчёт

struct Row {
    std::string kind, name, input, expected, got;
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
#define CHECK(cond) do { ++total; if (!(cond)) { ++failed; std::cerr << "  FAIL " << __FILE__ << ":" << __LINE__ << ": " #cond "\n"; } } while (0)

static const funnylex::Lexer& lexer() {
    static funnylex::Lexer l(funnylex::buildMinimalDFA(funnylex::loadTokenDefs(kTokens)));
    return l;
}

// Полный конвейер: парсер HW2 -> проверка HW3.
struct Checked {
    ParseResult parse;
    SemaResult sema;
    bool syntaxOk() const { return parse.ok(); }
};
static Checked check(const std::string& src) {
    Checked c;
    c.parse = parseProgram(src, lexer());
    if (c.parse.ok()) c.sema = analyze(*c.parse.ast);
    return c;
}

// Ошибки в виде "строка:столбец: сообщение", по одной на строку. Синтаксические — с пометкой "syntax: ".
static std::string errorsOf(const Checked& c) {
    std::string s;
    if (!c.syntaxOk()) {
        for (const auto& d : c.parse.errors) s += "syntax: " + std::to_string(d.line) + ":" + std::to_string(d.col) + ": " + d.message + "\n";
        return s;
    }
    for (const auto& d : c.sema.errors) s += std::to_string(d.line) + ":" + std::to_string(d.col) + ": " + d.message + "\n";
    return s;
}

// good: ошибок нет.
static void Good(const char* name, const std::string& src) {
    auto c = check(src);
    record("good", name, src, "(no errors)", c.syntaxOk() && c.sema.ok() ? "(no errors)" : errorsOf(c));
}
// bad: ожидаемый список ошибок.
static void Bad(const char* name, const std::string& src, const std::string& want) {
    auto c = check(src);
    std::string got = errorsOf(c);
    record("bad", name, src, want, got.empty() ? "(no errors)" : got);
}
// typed: аннотированный AST всей программы.
static void Typed(const char* name, const std::string& src, const std::string& want) {
    auto c = check(src);
    record("typed", name, src, want, c.syntaxOk() ? (c.sema.ok() ? "" : "ERR " + errorsOf(c)) + dumpAnnotatedSexpr(*c.parse.ast, c.sema) : "ERR " + errorsOf(c));
}
static void Ok(const char* kind, const char* name, const std::string& input, const std::string& want, const std::string& got) {
    record(kind, name, input, want, got);
}

// Программа-обёртка: f с параметрами a: int, b: int[], c: int и результатом r: int; defs — определения перед f.
static const std::string kWrapHead = "f(a: int, b: int[], c: int) returns r: int ";
// Типизированное выражение справа от "r = ".
static std::string exprT(const std::string& e, const std::string& defs = "") {
    auto c = check(defs + kWrapHead + "ensures true r = " + e + ";");
    if (!c.syntaxOk() || !c.sema.ok()) return "ERR " + errorsOf(c);
    const Node* f = c.parse.ast->kids.back().get();
    return dumpAnnotatedSexpr(*f->find(Kind::Assign)->kids[0], c.sema);
}
static std::string condT(const std::string& cnd) {
    auto c = check(kWrapHead + "ensures true if (" + cnd + ") r = 1;");
    if (!c.syntaxOk() || !c.sema.ok()) return "ERR " + errorsOf(c);
    return dumpAnnotatedSexpr(*c.parse.ast->kids.back()->find(Kind::If)->kids[0], c.sema);
}
static std::string predT(const std::string& p, const std::string& defs = "") {
    auto c = check(defs + kWrapHead + "ensures " + p + " r = 1;");
    if (!c.syntaxOk() || !c.sema.ok()) return "ERR " + errorsOf(c);
    return dumpAnnotatedSexpr(*c.parse.ast->kids.back()->find(Kind::Ensures)->kids[0], c.sema);
}
static void E(const char* name, const std::string& e, const std::string& want) { record("typed", name, e, want, exprT(e)); }
static void C(const char* name, const std::string& e, const std::string& want) { record("typed", name, e, want, condT(e)); }
static void P(const char* name, const std::string& e, const std::string& want) { record("typed", name, e, want, predT(e)); }

// Тело функции: заголовок (1 строка) + "{" + операторы со 3-й строки + "}".
// Параметры: a: int, b: int[]; возвраты: r: int, s: int[]; локальная: i: int.
static const std::string kHdr = "f(a: int, b: int[]) returns r: int, s: int[] uses i: int\n{\n";
static std::string body(const std::string& stmts) { return kHdr + stmts + "}\n"; }
static void BadS(const char* name, const std::string& stmts, const std::string& want) { Bad(name, body(stmts), want); }
static void GoodS(const char* name, const std::string& stmts) { Good(name, body(stmts)); }

// ---------- 1. таблицы символов: имена и области видимости ----------

static void test_names() {
    BadS("unknown_variable_rhs", "  r = zz;\n", "3:7: unknown variable 'zz'\n");
    BadS("unknown_variable_lhs", "  zz = 1;\n", "3:3: unknown variable 'zz'\n");
    BadS("unknown_array_in_index", "  r = zz[0];\n", "3:7: unknown variable 'zz'\n");
    BadS("unknown_array_in_store", "  zz[0] = 1;\n", "3:3: unknown variable 'zz'\n");
    BadS("unknown_in_condition", "  if (zz > 0) r = 1;\n", "3:7: unknown variable 'zz'\n");
    BadS("unknown_in_assert", "  assert zz > 0;\n", "3:10: unknown variable 'zz'\n");
    BadS("unknown_function", "  r = g(1);\n", "3:7: unknown function 'g'\n");
    BadS("unknown_function_and_variable", "  r = g(zz);\n", "3:7: unknown function 'g'\n3:9: unknown variable 'zz'\n");
    Bad("unknown_formula", "f() returns r: int ensures zz(1) r = 1;", "1:28: unknown formula 'zz'\n");
    Bad("unknown_variable_in_formula_body", "p(x: int) => y > 0", "1:14: unknown variable 'y'\n");
    Bad("variable_names_are_case_sensitive", "f(x: int) returns r: int r = X;", "1:30: unknown variable 'X'\n");

    // повторные объявления
    Bad("duplicate_parameter", "f(x: int, x: int) returns r: int r = x;", "1:11: duplicate declaration of 'x': already declared as parameter at 1:3\n");
    Bad("duplicate_param_and_return", "f(x: int) returns x: int r = 1;",
        "1:19: duplicate declaration of 'x': already declared as parameter at 1:3\n1:26: unknown variable 'r'\n");
    Bad("duplicate_return_and_local", "f() returns r: int uses r: int r = 1;", "1:25: duplicate declaration of 'r': already declared as return value at 1:13\n");
    Bad("duplicate_return_return", "f() returns r: int, r: int[] r = 1;", "1:21: duplicate declaration of 'r': already declared as return value at 1:13\n");
    Bad("duplicate_local_local", "f() returns r: int uses i: int, i: int r = 1;", "1:33: duplicate declaration of 'i': already declared as local variable at 1:25\n");
    Bad("duplicate_param_and_local", "f(i: int) returns r: int uses i: int r = i;", "1:31: duplicate declaration of 'i': already declared as parameter at 1:3\n");
    Bad("duplicate_formula_parameter", "p(x: int, x: int[]) => x > 0", "1:11: duplicate declaration of 'x': already declared as parameter at 1:3\n");
    Bad("duplicate_function", "f() returns r: int r = 1;\nf() returns r: int r = 2;",
        "2:1: redefinition of 'f': already defined as function at 1:1 (names must be unique in a module, no overloads)\n");
    Bad("no_overloading_by_parameters", "f(x: int) returns r: int r = x;\nf(x: int[]) returns r: int r = 1;",
        "2:1: redefinition of 'f': already defined as function at 1:1 (names must be unique in a module, no overloads)\n");
    Bad("function_and_formula_same_name", "p(x: int) => x > 0\np() returns r: int r = 1;",
        "2:1: redefinition of 'p': already defined as formula at 1:1 (names must be unique in a module, no overloads)\n");
    Bad("duplicate_formula", "p(x: int) => x > 0\np(x: int) => x > 1",
        "2:1: redefinition of 'p': already defined as formula at 1:1 (names must be unique in a module, no overloads)\n");
    Bad("body_of_duplicate_is_still_checked", "f() returns r: int r = 1;\nf() returns r: int r = zz;",
        "2:1: redefinition of 'f': already defined as function at 1:1 (names must be unique in a module, no overloads)\n2:24: unknown variable 'zz'\n");
    // одно и то же имя в разных функциях — не повтор
    Good("same_names_in_different_functions", "f(x: int) returns r: int r = x;\ng(x: int) returns r: int r = x;");
    // имена переменных и функций в разных пространствах: вызов всегда с круглыми скобками
    Good("variable_named_like_a_function", "g() returns r: int r = 1;\nf() returns g: int g = g();");
    Good("parameter_named_like_a_formula", "p(x: int) => x > 0\nf(p: int) returns r: int ensures p > 0 r = p;");

    // использование до объявления: функции видны во всём модуле, а вот локальные — не во всех предикатах
    Good("call_of_function_defined_below", "f() returns r: int r = g(1);\ng(x: int) returns q: int q = x;");
    Good("recursion", "fact(n: int) returns r: int uses t: int { if (n <= 1) r = 1; else { t = fact(n - 1); r = n * t; } }");
    Good("mutual_recursion", "even(n: int) returns r: int { if (n == 0) r = 1; else r = odd(n - 1); }\nodd(n: int) returns r: int { if (n == 0) r = 0; else r = even(n - 1); }");
    Good("formula_used_before_definition", "f(x: int) returns r: int ensures big(x) r = x;\nbig(x: int) => x > 100");

    // области видимости предусловия, постусловия, инварианта
    Bad("requires_sees_only_parameters", "f(x: int) requires r > 0 returns r: int r = x;",
        "1:20: variable 'r' (return value declared at 1:34) is not visible in the precondition (requires), where only parameters are visible\n");
    Bad("requires_does_not_see_local", "f(x: int) requires i > 0 returns r: int uses i: int r = x;",
        "1:20: variable 'i' (local variable declared at 1:46) is not visible in the precondition (requires), where only parameters are visible\n");
    Bad("ensures_does_not_see_local", "f(x: int) returns r: int ensures i > 0 uses i: int r = x;",
        "1:34: variable 'i' (local variable declared at 1:45) is not visible in the postcondition (ensures), where only parameters and return values are visible\n");
    Bad("formula_body_sees_only_its_parameters", "f(x: int) returns r: int r = x;\np(y: int) => x > 0",
        "2:14: unknown variable 'x'\n");
    Good("ensures_sees_parameters_and_returns", "f(x: int) returns r: int ensures r == x r = x;");
    Good("invariant_sees_everything", "f(x: int) returns r: int uses i: int\n{ i = 0; r = 0; while (i < x) invariant 0 <= i and i <= x and r == i { r = r + 1; i = i + 1; } }");
    Good("assert_and_assume_see_everything", "f(x: int) returns r: int uses i: int { i = x; assume i > 0; r = i; assert r == x and i == x; }");
    Bad("invariant_unknown_variable", "f(x: int) returns r: int while (x > 0) invariant zz > 0 r = 1;", "1:50: unknown variable 'zz'\n");
    // квантор: имя должно быть уникально в области, где он стоит
    Bad("quantifier_duplicates_parameter_in_ensures", "f(x: int) returns r: int ensures forall (x: int | x > 0) r = 1;",
        "1:34: quantifier variable 'x' duplicates the parameter 'x' declared at 1:3 (names in a scope must be unique)\n");
    Bad("quantifier_duplicates_return_in_ensures", "f(x: int) returns r: int ensures exists (r: int | r > x) r = 1;",
        "1:34: quantifier variable 'r' duplicates the return value 'r' declared at 1:19 (names in a scope must be unique)\n");
    Bad("quantifier_duplicates_local_in_invariant", "f(x: int) returns r: int uses i: int while (x > 0) invariant forall (i: int | i > 0) r = 1;",
        "1:62: quantifier variable 'i' duplicates the local variable 'i' declared at 1:31 (names in a scope must be unique)\n");
    Bad("quantifier_duplicates_enclosing_quantifier", "f(x: int) returns r: int ensures forall (i: int | exists (i: int | i > 0)) r = 1;",
        "1:51: quantifier variable 'i' is already declared by an enclosing quantifier at 1:34\n");
    Bad("quantifier_duplicates_formula_parameter", "p(x: int) => forall (x: int | x > 0)",
        "1:14: quantifier variable 'x' duplicates the parameter 'x' declared at 1:3 (names in a scope must be unique)\n");
    Good("quantifier_may_reuse_return_name_in_requires", "f(x: int) requires forall (r: int | r > x) returns r: int r = x;");
    Good("quantifier_may_reuse_local_name_in_ensures", "f(x: int) returns r: int ensures forall (i: int | i > 0 or x > 0) uses i: int r = x;");
    Good("sibling_quantifiers_may_share_a_name", "f(x: int) returns r: int ensures forall (i: int | i > 0 or x > 0) and exists (i: int | i > x) r = x;");
    Bad("quantifier_variable_not_visible_after_it", "f(x: int) returns r: int ensures (forall (i: int | i > 0)) and i > 0 r = 1;",
        "1:64: unknown variable 'i'\n");
}

//  2. типы: выражения, операторы, вызовы

static void test_types() {
    // присваивания
    BadS("assign_array_to_int", "  r = b;\n", "3:7: type mismatch in assignment to 'r': expected int, found int[]\n");
    BadS("assign_int_to_array", "  s = a;\n", "3:7: type mismatch in assignment to 's': expected int[], found int\n");
    BadS("assign_array_to_local_int", "  i = s;\n", "3:7: type mismatch in assignment to 'i': expected int, found int[]\n");
    BadS("operator_error_is_reported_at_the_operator", "  r = b + 1;\n  r = 1 + 2 * b;\n",
         "3:9: operator '+' requires int operands, found int[] and int\n4:13: operator '*' requires int operands, found int and int[]\n");
    GoodS("assign_int_and_array", "  r = a + 1;\n  s = b;\n  i = r;\n");
    // локальные переменные только int
    Bad("local_array_not_allowed", "f() returns r: int uses a: int[] r = 1;", "1:25: local variable 'a' cannot have type int[] (local variables can only be int)\n");
    Good("return_array_allowed", "f(a: int[]) returns b: int[] b = a;");
    // арифметика
    BadS("add_array", "  r = b + 1;\n", "3:9: operator '+' requires int operands, found int[] and int\n");
    BadS("mul_two_arrays", "  r = b * b;\n", "3:9: operator '*' requires int operands, found int[] and int[]\n");
    BadS("sub_array_right", "  r = a - s;\n", "3:9: operator '-' requires int operands, found int and int[]\n");
    BadS("div_array", "  r = b / 2;\n", "3:9: operator '/' requires int operands, found int[] and int\n");
    BadS("neg_array", "  r = -b;\n", "3:7: unary '-' requires an int operand, found int[]\n");
    BadS("neg_array_in_expression", "  r = 1 + -b;\n", "3:11: unary '-' requires an int operand, found int[]\n");
    // сравнения
    BadS("compare_arrays", "  if (b == b) r = 1;\n", "3:9: operator '==' requires int operands, found int[] and int[]\n");
    BadS("compare_array_and_int", "  while (s < 1) r = 1;\n", "3:12: operator '<' requires int operands, found int[] and int\n");
    BadS("compare_in_assert", "  assert b != a;\n", "3:12: operator '!=' requires int operands, found int[] and int\n");
    // индексация
    BadS("index_of_int", "  r = a[0];\n", "3:7: 'a' is not an array (its type is int), it cannot be indexed\n");
    BadS("index_is_array", "  r = b[b];\n", "3:9: array index must be int, found int[]\n");
    BadS("index_expression_is_checked", "  r = b[a + b];\n", "3:11: operator '+' requires int operands, found int and int[]\n");
    BadS("store_into_int", "  r[0] = 1;\n", "3:3: 'r' is not an array (its type is int), it cannot be indexed\n");
    BadS("store_index_is_array", "  s[b] = 1;\n", "3:5: array index must be int, found int[]\n");
    BadS("store_value_is_array", "  s[0] = s;\n", "3:10: array element must be int, found int[]\n");
    GoodS("store_and_load", "  s = b;\n  s[a] = b[0] + 1;\n  r = s[a];\n  i = length(s) - 1;\n");
    // неизменяемость параметров
    BadS("assign_to_parameter", "  a = 1;\n", "3:3: cannot assign to parameter 'a' (parameters are read-only)\n");
    BadS("assign_to_array_parameter", "  b = s;\n", "3:3: cannot assign to parameter 'b' (parameters are read-only)\n");
    BadS("store_into_array_parameter", "  b[0] = 1;\n", "3:3: cannot modify array parameter 'b' (parameters, including arrays, are read-only)\n");
    BadS("parameter_assign_still_checks_rhs_type", "  a = b;\n", "3:3: cannot assign to parameter 'a' (parameters are read-only)\n3:7: type mismatch in assignment to 'a': expected int, found int[]\n");
    // литералы
    BadS("literal_too_big", "  r = 2147483648;\n", "3:7: integer literal 2147483648 is out of range for int (32-bit signed: 0..2147483647)\n");
    BadS("literal_way_too_big", "  r = 99999999999;\n", "3:7: integer literal 99999999999 is out of range for int (32-bit signed: 0..2147483647)\n");
    BadS("negative_literal_too_small", "  r = -2147483649;\n", "3:8: integer literal 2147483649 is out of range for int (32-bit signed: -2147483648..2147483647)\n");
    GoodS("literal_int_max_and_min", "  r = 2147483647;\n  r = -2147483648;\n  r = 0;\n");
    // встроенная length
    BadS("length_of_int", "  r = length(a);\n", "3:14: argument 1 of built-in 'length': expected int[], found int\n");
    BadS("length_no_args", "  r = length();\n", "3:7: built-in 'length' expects 1 argument(s), found 0\n");
    BadS("length_two_args", "  r = length(b, b);\n", "3:7: built-in 'length' expects 1 argument(s), found 2\n");
    GoodS("length_of_array", "  r = length(b) + length(s);\n");
}

static void test_calls() {
    const std::string g = "g(x: int, c: int[]) returns q: int q = x;\n";
    Bad("call_too_few_arguments", g + "f() returns r: int r = g(1);", "2:24: function 'g' expects 2 argument(s), found 1\n");
    Bad("call_too_many_arguments", g + "f(b: int[]) returns r: int r = g(1, b, 3);", "2:32: function 'g' expects 2 argument(s), found 3\n");
    Bad("call_no_arguments", g + "f() returns r: int r = g();", "2:24: function 'g' expects 2 argument(s), found 0\n");
    Bad("call_argument_types_swapped", g + "f(b: int[]) returns r: int r = g(b, 1);",
        "2:34: argument 1 of function 'g': expected int, found int[]\n2:37: argument 2 of function 'g': expected int[], found int\n");
    Bad("call_argument_expression_position", g + "f(b: int[]) returns r: int r = g(1 + 2, b + 1);",
        "2:43: operator '+' requires int operands, found int[] and int\n");
    Bad("call_argument_error_inside_argument", g + "f() returns r: int r = g(zz, zz);",
        "2:26: unknown variable 'zz'\n2:30: unknown variable 'zz'\n");
    Good("call_with_correct_arguments", g + "f(b: int[]) returns r: int r = g(1 + 2, b) * g(r, b);");
    Good("call_returns_array", "h(a: int[]) returns b: int[] b = a;\nf(a: int[]) returns r: int uses i: int { i = length(h(a)); r = i; }");
    Good("array_result_assigned", "h(a: int[]) returns b: int[] b = a;\nf(a: int[]) returns s: int[] s = h(a);");
    Bad("array_result_assigned_to_int", "h(a: int[]) returns b: int[] b = a;\nf(a: int[]) returns r: int r = h(a);",
        "2:32: type mismatch in assignment to 'r': expected int, found int[]\n");
    // функции с несколькими результатами
    const std::string d = "d() returns q: int, r: int q = 1;\n";
    Bad("multi_return_in_expression", d + "f() returns r: int r = d() + 1;",
        "2:24: function 'd' returns 2 values and cannot be used in an expression (use a tuple assignment: a, b = d(...);)\n");
    Bad("multi_return_single_assignment", d + "f() returns r: int r = d();",
        "2:24: function 'd' returns 2 values and cannot be used in an expression (use a tuple assignment: a, b = d(...);)\n");
    Bad("multi_return_as_argument", d + "g(x: int) returns r: int r = x;\nf() returns r: int r = g(d());",
        "3:26: function 'd' returns 2 values and cannot be used in an expression (use a tuple assignment: a, b = d(...);)\n");
    Bad("tuple_too_many_targets", d + "f() returns a: int, b: int, c: int\n{\n  a, b, c = d();\n}\n",
        "4:3: tuple assignment has 3 target(s), but 'd' returns 2 value(s)\n");
    Bad("tuple_from_single_return_function", "g(x: int) returns q: int q = x;\nf() returns a: int, b: int\n{\n  a, b = g(1);\n}\n",
        "4:3: tuple assignment has 2 target(s), but 'g' returns 1 value(s)\n");
    Bad("tuple_target_type_mismatch", "d() returns q: int, r: int[] q = 1;\nf() returns a: int, b: int\n{\n  a, b = d();\n}\n",
        "4:6: cannot assign value #2 of 'd' (type int[]) to 'b' of type int\n");
    Bad("tuple_assign_to_parameter", d + "f(x: int) returns a: int\n{\n  x, a = d();\n}\n",
        "4:3: cannot assign to parameter 'x' (parameters are read-only)\n");
    Bad("tuple_unknown_target_and_function", "f() returns a: int\n{\n  u, a = zz();\n}\n",
        "3:3: unknown variable 'u'\n3:10: unknown function 'zz'\n");
    Bad("tuple_argument_type", "d(x: int[]) returns q: int, r: int q = 1;\nf() returns a: int, b: int\n{\n  a, b = d(1);\n}\n",
        "4:12: argument 1 of function 'd': expected int[], found int\n");
    Good("tuple_assignment", d + "f() returns a: int, b: int uses t: int { a, b = d(); t, a = d(); }");
    Good("tuple_with_array_result", "d() returns q: int, r: int[] q = 1;\nf() returns a: int, b: int[] { a, b = d(); }");
    // формулы и функции — разные сущности
    const std::string p = "p(x: int) => x > 0\n";
    Bad("formula_in_expression", p + "f() returns r: int r = p(1);",
        "2:24: 'p' is a formula: formulas can be used only in predicates (requires, ensures, invariant, assert, assume, formula bodies)\n");
    Bad("formula_in_condition", p + "f() returns r: int if (p(1) > 0) r = 1;",
        "2:24: 'p' is a formula: formulas can be used only in predicates (requires, ensures, invariant, assert, assume, formula bodies)\n");
    Bad("function_as_predicate", "g(x: int) returns q: int q = x;\nf() returns r: int ensures g(1) r = 1;",
        "2:28: 'g' is a function, not a formula: a bare function call is not a predicate (compare its result, e.g. g(...) > 0)\n");
    Bad("formula_wrong_argument_count", p + "f() returns r: int ensures p(1, 2) r = 1;", "2:28: formula 'p' expects 1 argument(s), found 2\n");
    Bad("formula_wrong_argument_type", p + "f(b: int[]) returns r: int ensures p(b) r = 1;", "2:38: argument 1 of formula 'p': expected int, found int[]\n");
    Good("formula_in_predicates", "p(x: int, v: int[]) => x >= 0 and x < length(v)\nf(a: int, b: int[]) requires p(a, b) returns r: int ensures p(r, b) or not p(a, b) { assert p(a, b); r = b[a]; }");
    Good("function_in_predicate_comparison", "g(x: int) returns q: int q = x;\nf(a: int) returns r: int ensures g(a) > 0 and g(r) == a r = a;");
    Good("formula_calls_formula_and_function", "g(x: int) returns q: int q = x;\np(x: int) => x > 0\nq(x: int) => p(x) and g(x) > 1");
}

//  3. все ошибки сразу, а не только первая

static void test_multiple_errors() {
    Bad("several_errors_in_one_function",
        "f(x: int, x: int[]) returns r: int ensures true uses r: int\n{\n  y = 1;\n  x = 2;\n  r = x + 1;\n  r = g(1);\n  r = f(1);\n  r = length(r);\n}\n",
        "1:11: duplicate declaration of 'x': already declared as parameter at 1:3\n"
        "1:54: duplicate declaration of 'r': already declared as return value at 1:29\n"
        "3:3: unknown variable 'y'\n4:3: cannot assign to parameter 'x' (parameters are read-only)\n"
        "6:7: unknown function 'g'\n7:7: function 'f' expects 2 argument(s), found 1\n8:14: argument 1 of built-in 'length': expected int[], found int\n");
    Bad("errors_in_different_functions",
        "f() returns r: int r = zz;\ng() returns r: int r = ww;\nh() returns r: int r = vv;\n",
        "1:24: unknown variable 'zz'\n2:24: unknown variable 'ww'\n3:24: unknown variable 'vv'\n");
    BadS("errors_in_nested_statements",
         "  if (zz > 0) { r = b; } else { s = a; }\n  while (ww > 0) { a = 1; }\n",
         "3:7: unknown variable 'zz'\n3:21: type mismatch in assignment to 'r': expected int, found int[]\n"
         "3:37: type mismatch in assignment to 's': expected int[], found int\n4:10: unknown variable 'ww'\n4:20: cannot assign to parameter 'a' (parameters are read-only)\n");
    Bad("errors_in_header_and_body",
        "f(x: int) requires zz > 0 returns r: int ensures ww > 0 { r = vv; }",
        "1:20: unknown variable 'zz'\n1:50: unknown variable 'ww'\n1:63: unknown variable 'vv'\n");
    // после ошибки тип узла — error, он совместим со всем: одна причина — одна диагностика
    BadS("no_cascade_after_unknown_variable", "  r = zz + 1;\n", "3:7: unknown variable 'zz'\n");
    BadS("no_cascade_in_condition", "  if (zz + 1 > a and b[zz] > 0) r = 1;\n", "3:7: unknown variable 'zz'\n3:24: unknown variable 'zz'\n");
    BadS("no_cascade_after_bad_operator", "  r = b + 1;\n  s = r;\n", "3:9: operator '+' requires int operands, found int[] and int\n4:7: type mismatch in assignment to 's': expected int[], found int\n");
    BadS("no_cascade_unknown_lhs", "  zz = b;\n", "3:3: unknown variable 'zz'\n");
    BadS("no_cascade_after_unknown_call", "  r = g(1) + g(2);\n", "3:7: unknown function 'g'\n3:14: unknown function 'g'\n");
    // синтаксические ошибки останавливают конвейер до проверки типов
    Bad("syntax_errors_come_first", "f() returns r: int ensures true r = ;", "syntax: 1:37: expected expression, found ';'\n");
    Bad("syntax_error_hides_semantic_errors", "f() returns r: int ensures zz > 0 r = ;", "syntax: 1:39: expected expression, found ';'\n");
}

//  4. корректные программы

static void test_good_programs() {
    Good("gcd", "gcd(x: int, y: int) requires x > 0 and y > 0 returns r: int ensures r > 0 uses a: int, b: int\n"
                "{ a = x; b = y; while (a != b) invariant a > 0 and b > 0 { if (a > b) a = a - b; else b = b - a; } r = a; }");
    Good("divide_and_use", "divide(a: int, b: int) requires b > 0 returns q: int, r: int ensures a == q * b + r and 0 <= r and r < b\n"
                           "{ q = 0; r = a; while (r >= b) { r = r - b; q = q + 1; } }\n"
                           "useDivide(n: int) returns s: int ensures true uses q: int, r: int { q, r = divide(n, 3); s = q + r; }");
    Good("sum_of_array", "sum(a: int[]) returns s: int ensures s >= 0 uses i: int\n"
                         "{ s = 0; i = 0; while (i < length(a)) { s = s + a[i]; i = i + 1; } }");
    Good("array_update_copy", "fill(a: int[], n: int) returns b: int[] ensures length(b) == length(a) uses i: int\n"
                              "{ b = a; i = 0; while (i < n) invariant 0 <= i { b[i] = a[i + 1] * 2; i = i + 1; } }");
    Good("formulas_and_quantifiers",
         "sorted(a: int[], n: int) => forall (i: int | i < 0 or i + 1 >= n or a[i] <= a[i + 1])\n"
         "hasZero(a: int[]) => exists (i: int | i >= 0 and i < length(a) and a[i] == 0)\n"
         "first(a: int[]) requires sorted(a, length(a)) returns r: int ensures not hasZero(a) or r == 0 { r = a[0]; }");
    Good("array_quantifier_variable", "f(a: int[]) returns r: int ensures exists (c: int[] | length(c) == length(a)) r = 0;");
    Good("implication_in_conditions", "f(x: int) returns r: int { if (x > 0 -> x > 1 -> x > 2) r = x; else r = 0; }");
    Good("nested_blocks_and_else_if", "f(x: int) returns r: int { { { r = 0; } } if (x > 0) r = 1; else if (x < 0) r = -1; else r = 2; }");
    Good("read_before_assignment_is_allowed", "f() returns r: int uses t: int { r = t + 1; }");
    Good("unused_variables_are_allowed", "f(a: int, b: int[]) returns r: int, s: int[] uses i: int, j: int { r = 1; }");
    Good("missing_ensures_is_allowed", "f() returns r: int r = 1;");
}

//  5. аннотированный AST

static void test_typed_ast() {
    E("typed_precedence", "1 + 2 * 3", "(+:int 1:int (*:int 2:int 3:int))");
    E("typed_unary_minus", "a - -c", "(-:int a:int (Neg:int c:int))");
    E("typed_int_min", "-2147483648", "(Neg:int 2147483648:int)");
    E("typed_index", "b[a + 1]", "(Index:int b (+:int a:int 1:int))");
    E("typed_length", "length(b) - 1", "(-:int (Call:int length b:int[]) 1:int)");
    E("typed_return_variable", "r", "r:int");
    record("typed", "typed_user_call_known", "g(x: int, v: int[]) ...; r = g(a, b) * 2", "(*:int (Call:int g a:int b:int[]) 2:int)",
           exprT("g(a, b) * 2", "g(x: int, v: int[]) returns q: int q = x;\n"));
    record("typed", "typed_call_returning_array_in_length", "h(v: int[]) ...; r = length(h(b))", "(Call:int length (Call:int[] h b:int[]))",
           exprT("length(h(b))", "h(v: int[]) returns w: int[] w = v;\n"));
    C("typed_condition", "(a > 0 or b[0] == c) and not a < c",
      "(And:bool (Or:bool (>:bool a:int 0:int) (==:bool (Index:int b 0:int) c:int)) (Not:bool (<:bool a:int c:int)))");
    C("typed_implication", "a > 0 -> c > 0", "(Implies:bool (>:bool a:int 0:int) (>:bool c:int 0:int))");
    C("typed_bool_constants", "true and not false", "(And:bool (BoolConst:bool true) (Not:bool (BoolConst:bool false)))");
    P("typed_forall", "forall (i: int | i < 0 or i >= length(b) or b[i] >= 0)",
      "(Forall:bool i int (Or:bool (Or:bool (<:bool i:int 0:int) (>=:bool i:int (Call:int length b:int[]))) (>=:bool (Index:int b i:int) 0:int)))");
    P("typed_exists_array_variable", "exists (d: int[] | length(d) == a)", "(Exists:bool d int[] (==:bool (Call:int length d:int[]) a:int))");
    record("typed", "typed_formula_ref", "p(x: int, v: int[]) => ...; ensures p(a, b)", "(FormulaRef:bool p a:int b:int[])",
           predT("p(a, b)", "p(x: int, v: int[]) => x > 0\n"));
    Typed("typed_function_with_statements",
          "f(a: int[]) returns b: int[] { b = a; b[0] = 1; }",
          "(Program (Function f (Params (VarDef a int[])) (Returns (VarDef b int[])) (Block (Assign:int[] b a:int[]) (ArrayAssign:int[] b 0:int 1:int))))");
    Typed("typed_tuple_assignment",
          "d() returns q: int, r: int q = 1;\nf() returns a: int, b: int { a, b = d(); }",
          "(Program (Function d (Params) (Returns (VarDef q int) (VarDef r int)) (Assign:int q 1:int)) (Function f (Params) (Returns (VarDef a int) (VarDef b int)) (Block (TupleAssign (Targets a:int b:int) (Call:(int,int) d)))))");
    Typed("typed_while_and_if",
          "f(x: int) returns r: int { while (r < x) invariant r >= 0 r = r + 1; }",
          "(Program (Function f (Params (VarDef x int)) (Returns (VarDef r int)) (Block (While (<:bool r:int x:int) (Invariant (>=:bool r:int 0:int)) (Assign:int r (+:int r:int 1:int))))))");
    // узлы с ошибкой помечаются типом error, но остаются в дереве
    Typed("typed_error_nodes_are_marked", "f() returns r: int r = zz;",
          "ERR 1:24: unknown variable 'zz'\n(Program (Function f (Params) (Returns (VarDef r int)) (Assign:int r zz:error)))");
}

static bool has(const std::string& s, const std::string& part) { return s.find(part) != std::string::npos; }

static void test_annotation_format() {
    const std::string src = "inc(x: int) returns r: int r = x + 1;";
    auto c = check(src);
    std::string js = dumpAnnotatedJSON(*c.parse.ast, c.sema);
    Ok("annot", "json_function_has_signature", src, "yes",
       has(js, "{\"kind\": \"Function\", \"text\": \"inc\", \"line\": 1, \"col\": 1, \"symbol\": \"function\", \"signature\": \"(int)->(int)\"") ? "yes" : "no: " + js);
    Ok("annot", "json_var_has_type_and_role", src, "yes",
       has(js, "{\"kind\": \"Var\", \"text\": \"x\", \"line\": 1, \"col\": 32, \"type\": \"int\", \"symbol\": \"param\"}") ? "yes" : "no: " + js);
    Ok("annot", "json_binary_has_type_at_operator", src, "yes",
       has(js, "{\"kind\": \"Binary\", \"text\": \"+\", \"line\": 1, \"col\": 34, \"type\": \"int\"") ? "yes" : "no: " + js);
    Ok("annot", "json_vardef_has_declared_type_and_role", src, "yes",
       has(js, "{\"kind\": \"VarDef\", \"text\": \"r\", \"line\": 1, \"col\": 21, \"type\": \"int\", \"symbol\": \"return\"}") ? "yes" : "no: " + js);

    const std::string src2 = "f(a: int[]) returns r: int r = length(a);";
    auto c2 = check(src2);
    std::string js2 = dumpAnnotatedJSON(*c2.parse.ast, c2.sema);
    Ok("annot", "json_builtin_call", src2, "yes",
       has(js2, "\"kind\": \"Call\", \"text\": \"length\"") && has(js2, "\"type\": \"int\", \"symbol\": \"builtin\"") ? "yes" : "no: " + js2);

    const std::string src3 = "p(x: int) => forall (i: int | i > x)";
    auto c3 = check(src3);
    std::string js3 = dumpAnnotatedJSON(*c3.parse.ast, c3.sema);
    Ok("annot", "json_formula_and_quantifier", src3, "yes",
       has(js3, "\"signature\": \"(int)->bool\"") && has(js3, "\"kind\": \"Forall\", \"text\": \"i\"") && has(js3, "\"type\": \"bool\"") &&
               has(js3, "\"var_type\": \"int\"") && has(js3, "\"symbol\": \"quantifier\"") ? "yes" : "no: " + js3);

    const std::string src4 = "d() returns q: int, r: int[] q = 1;\nf() returns a: int, b: int[] { a, b = d(); }";
    auto c4 = check(src4);
    std::string js4 = dumpAnnotatedJSON(*c4.parse.ast, c4.sema);
    Ok("annot", "json_tuple_type", src4, "yes", has(js4, "\"type\": \"(int,int[])\", \"symbol\": \"function\"") ? "yes" : "no: " + js4);

    // дерево с отступами
    {
        auto cc = check("inc(x: int) returns r: int r = x;");
        Ok("annot", "tree_format", "inc(x: int) returns r: int r = x;",
           "Program\n  Function inc  (int)->(int)\n    Params\n      VarDef x: int\n    Returns\n      VarDef r: int\n    Assign r -> int\n      Var x -> int\n",
           dumpAnnotatedTree(*cc.parse.ast, cc.sema));
    }

    // таблица функций модуля (для следующих этапов)
    auto c5 = check("g(x: int, v: int[]) returns q: int, w: int[] q = x;\np(x: int) => x > 0\n");
    std::string tbl;
    for (const auto& kv : c5.sema.functions) {
        tbl += kv.first + (kv.second.isFormula ? " formula " : " function ") + std::to_string(kv.second.params.size()) + " params, " +
               std::to_string(kv.second.returns.size()) + " returns, at " + std::to_string(kv.second.line) + ":" + std::to_string(kv.second.col) + "\n";
    }
    Ok("annot", "function_table", "g(...) ...; p(...) => ...",
       "g function 2 params, 2 returns, at 1:1\np formula 1 params, 0 returns, at 2:1\n", tbl);

    // аннотация есть у каждого узла выражения без ошибок
    {
        auto cc = check("f(a: int, b: int[]) returns r: int ensures r >= 0 and forall (i: int | i < length(b) or b[i] == a) uses j: int\n"
                        "{ j = 0; while (j < a) invariant j >= 0 { r = r + b[j] * 2 - -a; j = j + 1; } assert r == r; }");
        int exprs = 0, typedCount = 0;
        std::vector<const Node*> st{cc.parse.ast.get()};
        while (!st.empty()) {
            const Node* n = st.back();
            st.pop_back();
            switch (n->kind) {
                case Kind::Number: case Kind::Var: case Kind::Neg: case Kind::Binary: case Kind::Call: case Kind::Index:
                case Kind::BoolConst: case Kind::Compare: case Kind::Not: case Kind::And: case Kind::Or: case Kind::Implies:
                case Kind::Forall: case Kind::Exists: case Kind::FormulaRef:
                    ++exprs;
                    if (cc.sema.at(*n).type != Ty::None && cc.sema.at(*n).type != Ty::Error) ++typedCount;
                    break;
                default: break;
            }
            for (const auto& k : n->kids) st.push_back(k.get());
        }
        Ok("annot", "every_expression_node_has_a_type", "a program with ~30 expression nodes", "ok=" + std::string(cc.sema.ok() ? "yes" : "no") + " typed=" + std::to_string(exprs) + "/" + std::to_string(exprs),
           "ok=" + std::string(cc.sema.ok() ? "yes" : "no") + " typed=" + std::to_string(typedCount) + "/" + std::to_string(exprs));
    }
    // позиции ошибок: строка и столбец с 1, столбцы в байтах
    Bad("error_position_multiline", "f() returns r: int\n{\n  r =\n     zz;\n}\n", "4:6: unknown variable 'zz'\n");
    Bad("error_position_crlf", "f() returns r: int\r\n{\r\n  r = zz;\r\n}\r\n", "3:7: unknown variable 'zz'\n");
    Bad("error_position_after_non_ascii_comment", "// \xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82\nf() returns r: int r = zz;", "2:24: unknown variable 'zz'\n");
    // формат диагностики с кареткой (используется CLI)
    {
        auto cc = check("f() returns r: int r = zz;\n");
        std::string got = formatDiagnostic("f() returns r: int r = zz;\n", cc.sema.errors[0], "a.fn");
        Ok("annot", "diagnostic_text_with_caret", "f() returns r: int r = zz;",
           "a.fn:1:24: error: unknown variable 'zz'\n    f() returns r: int r = zz;\n                           ^\n", got);
    }
}

//  6. устойчивость

static unsigned rngState = 4242;
static unsigned rnd(unsigned n) { rngState = rngState * 1664525u + 1013904223u; return (rngState >> 8) % n; }

static void test_robustness() {
    const std::string survives = "no crash, errors sorted by position";
    const char* progs[] = {
        "gcd(x: int, y: int) requires x > 0 returns r: int ensures r > 0 uses a: int, b: int { a = x; b = y; while (a != b) invariant a > 0 { if (a > b) a = a - b; else b = b - a; } r = a; }",
        "d(a: int, b: int) returns q: int, r: int { q = a / b; r = a - q * b; }\nf(n: int) returns s: int uses q: int, r: int { q, r = d(n, 3); s = q + r; }",
        "p(a: int[], n: int) => forall (i: int | i < 0 or i + 1 >= n or a[i] <= a[i + 1])\nf(a: int[]) requires p(a, length(a)) returns b: int[] uses i: int { b = a; i = 0; while (i < length(a)) { b[i] = a[i] + 1; i = i + 1; } }",
    };
    auto sorted = [](const SemaResult& r) {
        for (size_t i = 1; i < r.errors.size(); ++i)
            if (r.errors[i - 1].line > r.errors[i].line || (r.errors[i - 1].line == r.errors[i].line && r.errors[i - 1].col > r.errors[i].col)) return false;
        return true;
    };
    bool allSorted = true;
    int runs = 0, analysed = 0;
    for (const char* src : progs) {
        std::string s = src;
        for (size_t n = 0; n <= s.size(); ++n) {
            auto c = check(s.substr(0, n));
            ++runs;
            if (c.syntaxOk()) { ++analysed; allSorted &= sorted(c.sema); }
        }
    }
    CHECK(allSorted);
    Ok("robust", "every_prefix_of_good_programs", "all prefixes of 3 programs (" + std::to_string(runs) + " runs, " + std::to_string(analysed) + " reach type checking)", survives, allSorted ? survives : "errors not sorted");
    {
        // мутации заменой одного слова на другое имя/тип: парсится, но проверка должна найти ошибки и не упасть
        const char* swaps[] = {"x", "a", "r", "int[]", "b", "zz", "length", "0", "d", "p"};
        const char* words[] = {"x", "a", "b", "r", "q", "i", "n", "int", "d", "p"};
        int n = 0;
        bool sortedAll = true;
        for (int it = 0; it < 3000; ++it) {
            std::string s = progs[rnd(3)];
            for (int k = 0, m = 1 + rnd(3); k < m; ++k) {
                const std::string w = words[rnd(10)];
                size_t at = s.find(w, rnd(s.size()));
                if (at == std::string::npos) continue;
                bool wordStart = at == 0 || !isalnum(static_cast<unsigned char>(s[at - 1]));
                bool wordEnd = at + w.size() >= s.size() || !isalnum(static_cast<unsigned char>(s[at + w.size()]));
                if (wordStart && wordEnd) s.replace(at, w.size(), swaps[rnd(10)]);
            }
            auto c = check(s);
            ++n;
            if (c.syntaxOk()) sortedAll &= sorted(c.sema);
        }
        CHECK(sortedAll);
        Ok("robust", "random_renames_of_identifiers", "3000 programs with 1-3 random renames", survives, sortedAll ? survives : "errors not sorted");
    }
    {
        // глубокая вложенность выражения: проверка (рекурсивная) обходит то, что разобрал парсер (глубина <= 200)
        std::string deep(150, '(');
        auto c = check("f() returns r: int r = " + deep + "zz" + std::string(150, ')') + ";");
        Ok("robust", "deep_expression_150", "150 x '(' around an unknown variable", "1 semantic error", c.syntaxOk() ? std::to_string(c.sema.errors.size()) + " semantic error" : "syntax");
        std::string e = "1";
        for (int i = 0; i < 150; ++i) e += " + b";
        auto c2 = check("f(b: int[]) returns r: int r = " + e + ";");
        Ok("robust", "long_left_chain_150_errors", "1 + b + b + ... (150 times)", "150 semantic error", c2.syntaxOk() ? std::to_string(c2.sema.errors.size()) + " semantic error" : "syntax");
    }
}

//  отчёт

static size_t utf8Len(const std::string& s, size_t i) {
    unsigned char c = s[i];
    size_t n = c >= 0xF0 && c < 0xF8 ? 4 : c >= 0xE0 ? (c < 0xF0 ? 3 : 1) : c >= 0xC2 && c < 0xE0 ? 2 : 1;
    if (n == 1 || i + n > s.size()) return 1;
    for (size_t k = 1; k < n; ++k)
        if ((static_cast<unsigned char>(s[i + k]) & 0xC0) != 0x80) return 1;
    return n;
}
static std::string cell(const std::string& s, size_t limit = 240) {
    std::string o;
    size_t shown = 0;
    for (size_t i = 0; i < s.size(); ++shown) {
        if (shown >= limit) { o += "…(" + std::to_string(s.size()) + " chars)"; break; }
        unsigned char c = s[i];
        size_t n = utf8Len(s, i);
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
static std::string cellErrors(const std::string& s) {
    std::string out, line;
    std::istringstream in(s);
    bool first = true;
    while (std::getline(in, line)) { out += (first ? "" : "<br>") + cell(line); first = false; }
    return out.empty() ? cell(s) : out;
}

static void writeReport(const std::string& path) {
    std::map<std::string, int> count;
    int passed = 0;
    for (auto& r : rows) { count[r.kind]++; passed += r.pass; }
    std::ofstream o(path, std::ios::binary);
    o << "# HW3 report\n\n";
    o << "Semantic analysis of Funny: symbol tables with scopes, type checking of expressions, statements and calls, "
         "diagnostics with positions, annotated AST. Input is the AST of the HW2 parser (HW1 lexer underneath); "
         "types and limitations are described in `docs/types.md`, the annotated AST format in `docs/annotated_ast.md`.\n\n";
    o << "## Tests: " << passed << "/" << rows.size() << " passed (good " << count["good"] << ", typed " << count["typed"] << ", bad " << count["bad"]
      << ", annot " << count["annot"] << ", robust " << count["robust"] << ")\n\n";
    o << "`good` — program is correct, no errors are reported; "
         "`typed` — the annotated AST (S-expression, `node:type`) must match the expected one; "
         "`bad` — must give exactly the expected diagnostics (`line:col: message`; `syntax:` marks a parser error); "
         "`annot` — format of the annotated AST (JSON, tree, function table) and of diagnostics; "
         "`robust` — the checker must not crash on broken programs.\n\n";
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
         "Результат не сверяется с эталоном: проверяется, что конвейер «парсер -> проверка» не падает и не зависает, а список ошибок остаётся "
         "отсортированным по позиции. Генераторы случайных чисел используют фиксированное начальное значение, поэтому каждый прогон одинаков.\n\n"
         "- **Все префиксы хороших программ.** Программа режется на куски любой длины; те куски, которые парсер принял без ошибок, идут в проверку типов.\n"
         "- **Случайные замены идентификаторов** (3000 запусков). В хорошей программе 1-3 слова заменяются другими именами, типом или `length`: "
         "программа остаётся синтаксически верной, но в ней появляются неизвестные имена, неверные типы и числа аргументов.\n"
         "- **Глубина.** 150 скобок вокруг выражения и цепочка из 150 ошибочных сложений: ошибки не теряются и не дублируются.\n";
}

int main() {
    struct { const char* name; void (*fn)(); } all[] = {
        {"names: symbol tables, scopes, duplicates", test_names},
        {"types: expressions, statements, immutability", test_types},
        {"calls: arguments, tuples, formulas", test_calls},
        {"several errors at once, no cascades", test_multiple_errors},
        {"correct programs", test_good_programs},
        {"annotated AST: types of nodes", test_typed_ast},
        {"annotated AST: formats and positions", test_annotation_format},
        {"robustness", test_robustness},
    };
    for (auto& t : all) {
        int before = failed;
        try { t.fn(); } catch (const std::exception& ex) { ++failed; std::cerr << "  EXCEPTION: " << ex.what() << "\n"; }
        std::cout << (failed == before ? "[ OK ] " : "[FAIL] ") << t.name << "\n";
    }
    const char* rp = std::getenv("FUNNY_REPORT");
    std::string path = rp ? rp : "REPORT_HW3.md";
    if (!path.empty()) writeReport(path);
    std::cout << (total - failed) << "/" << total << " checks passed";
    if (!path.empty()) std::cout << "; report: " << path;
    std::cout << "\n";
    return failed ? 1 : 0;
}
