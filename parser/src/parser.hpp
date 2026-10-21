#pragma once
#include <string>
#include <vector>

#include "ast.hpp"
#include "funnylex.hpp"

namespace funnyparse {

// Одна ошибка (лексическая или синтаксическая) с позицией (строка и столбец с 1, столбец в байтах).
struct Diagnostic {
    int line = 0, col = 0;
    std::string message;
};

struct ParseResult {
    NodePtr ast;                       // всегда не null; при ошибках — только успешно разобранные части
    std::vector<Diagnostic> errors;    // отсортированы по позиции
    bool ok() const { return errors.empty(); }
};

// Разбирает программу Funny: лексер из HW1 -> токены с позициями -> рекурсивный спуск (LL(1)).
// Ошибки собираются в режиме паники; функция всегда завершается.
ParseResult parseProgram(const std::string& source, const funnylex::Lexer& lexer);

// "файл:строка:столбец: error: сообщение" + строка исходника и каретка под ошибкой.
std::string formatDiagnostic(const std::string& source, const Diagnostic& d, const std::string& file);

}  // namespace funnyparse
